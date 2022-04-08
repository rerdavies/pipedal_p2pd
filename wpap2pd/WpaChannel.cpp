#include "includes/WpaChannel.h"
#include "ss.h"
#include "cotask/Os.h"

extern "C"
{
#include "wpa_ctrl.h"
}

using namespace p2p;
using namespace std;
using namespace cotask;

static const char *WPA_CONTROL_SOCKET_DIR = "/var/run/wpa_supplicant";

void WpaChannel::RequestOK(const std::string &message)
{
    auto response = Request(message);
    if (response.size() >= 1)
    {
        if (response[0] != "OK")
        {
            throw WpaIoException(EBADMSG, SS("Request failed. (" << response[0] << ") " << message));
        }
    }
    else
    {
        throw WpaIoException(EBADMSG, SS("Request failed. (No response). " << message));
    }
}

std::vector<std::string> WpaChannel::Request(
    const std::string &message)
{
    char reply[4096]; // the maximum length of a UNIX socket buffer.
    if (message.size() == 0 || message[message.length() - 1] != '\n')
    {
        throw invalid_argument("Message must end with '\\n'");
    }
    size_t len = sizeof(reply);
    if (traceMessages)
    {
        Log().Info(SS(logPrefix << "> " << message.substr(0, message.length() - 1)));
    }
    int retries = 0;
    while (true)
    {
        if (IsDisconnected())
        {
            throw WpaDisconnectedException();
        }
        errno = 0;
        int retVal;
        if (commandSocket == nullptr)
        {
            throw WpaIoException(EBADFD, "Socket not open.");
        }
        retVal = wpa_ctrl_request(commandSocket, message.c_str(), message.length() - 1, reply, &len, nullptr);
        if (retVal != 0)
        {
            if (retVal == -1 && errno == EAGAIN)
            {
                if (++retries < 3)
                {
                    os::msleep(100);
                    continue;
                }
                throw CoTimedOutException();
            }

            if (retVal == -2)
            {
                throw CoTimedOutException();
            }
            if (errno != 0)
            {
                CoIoException::ThrowErrno();
            }
            else
            {
                throw WpaIoException(EBADMSG, "Failed.");
            }
        }
        break;
    }
    reply[len] = 0;

    vector<string> result;
    const char *p = reply;
    while (*p != 0)
    {
        const char *start = p;
        while (*p != 0 && *p != '\n')
        {
            ++p;
        }
        std::string line(start, p);
        if (traceMessages)
        {
            Log().Info(SS(logPrefix << "< " << line));
        }
        result.push_back(line);
        if (*p == '\n')
            ++p;
    }
    if (result.size() == 1)
    {
        if (result[0] == "UNKNOWN COMMAND")
        {
            throw WpaIoException(EBADMSG, SS("Unknown wpa command: " << message));
        }
        // if (result[0] == "FAIL")
        // {
        //     throw WpaIoException(EBADMSG, SS("Command failed."));
        // }
    }
    return result;
}
void WpaChannel::SetDisconnected()
{
    cvDelay.NotifyAll([this]() {
        this->disconnected = true;
    });
}

WpaChannel::~WpaChannel()
{
    CloseChannel();
}

void WpaChannel::CloseChannel()
{
    if (closed)
        return;
    closed = true;
    if (withEvents)
    {
        try
        {
            SetDisconnected();
            if (cvRecvRunning.Test<bool>([this]() { return this->recvThreadCount != 0; }))
            {
                CoTask<> task = JoinRecvThread();
                try
                {
                    // Pumps messages until the recv thread has indicated that it has completed.
                    // we can't destruct until we're sure the recv thread won't access this.
                    task.GetResult();
                }
                catch (const std::exception &e)
                {
                    // task should NEVER throw.
                    Log().Error(SS(interfaceName << " Unexpected exception while terminating recv thread. " << e.what()));
                    std::terminate();
                }
            }
            // once more to make sure all the Disconnect exceptions have settled.
            Dispatcher().PumpMessages();

            CloseChannel(this->commandSocket);
            this->commandSocket = nullptr;
        }
        catch (const std::exception &e)
        {
            //~ is noexcept!
        }
    }
    else
    {
        if (this->commandSocket)
        {
            CloseChannel(this->commandSocket);
            this->commandSocket = nullptr;
        }
    }
}

void WpaChannel::OpenChannel(const std::string &interfaceName, bool withEvents)
{
    this->withEvents = withEvents;
    if (withEvents)
    {
        this->interfaceName = interfaceName;
        std::filesystem::path socketDir{WPA_CONTROL_SOCKET_DIR};

        std::filesystem::path p2pPath = socketDir / interfaceName;

        wpa_ctrl *pEventSocket;

        // seems to be a race condition between receiving group added event and avaialability of the socket.

        for (int retry = 0; true; ++retry)
        {
            try
            {
                pEventSocket = OpenWpaSocket(interfaceName);
            }
            catch (const WpaIoException &e)
            {
                if (retry == 3)
                {
                    throw WpaIoException(e.errNo(), SS("Can't open event socket. " << e.what()));
                }
                os::msleep(100);
                continue;
            }
            int result = wpa_ctrl_attach(pEventSocket);
            if (result != 0)
            {
                if (result == -2)
                {
                    wpa_ctrl_close(pEventSocket);
                    if (retry == 3)
                    {
                        auto message = SS("Timed out attaching to " << interfaceName);
                        throw WpaIoException(ETIMEDOUT, message);
                    }
                    auto message = SS("Timed out attaching to " << interfaceName << ". Retrying...");

                    Log().Warning(message);
                    continue;
                }
                else
                {
                    wpa_ctrl_close(pEventSocket);
                    throw CoIoException(errno, SS("Failed to attach to " << interfaceName));
                }
            }
            break;
        }

        closed = false;

        try
        {
            this->commandSocket = OpenWpaSocket(interfaceName);
        }
        catch (const WpaIoException &e)
        {
            throw WpaIoException(e.errNo(), SS("Can't open command socket. " << e.what()));
        }

        cvRecvRunning.Execute([this]() {
            this->recvThreadCount = 2;
        });
        Dispatcher().StartThread(ReadEventsProc(pEventSocket, interfaceName));

        Dispatcher().StartThread(ForegroundEventHandler());
    }
    else
    {
        closed = false;
        try
        {
            // race condition between Add group event and avaialability of the socket.
            for (int retry = 0; true; ++retry)
            {
                try
                {
                    this->commandSocket = OpenWpaSocket(interfaceName);
                }
                catch (const WpaIoException &e)
                {
                    if (retry == 3)
                    {
                        throw;
                    }
                    os::msleep(100);
                    continue;
                }
                break;
            }
        } catch (const WpaIoException &e)
        {
            throw WpaIoException(e.errNo(),SS("Can't open command socket. " << e.what()));
        }
    }
}

CoTask<> WpaChannel::ReadEventsProc(wpa_ctrl *ctrl, std::string interfaceName)
{
    co_await CoBackground();

    int ctrl_fd = -1;

    try
    {
        char buffer[4096];

        auto &asyncIo = AsyncIo::GetInstance();

        ctrl_fd = wpa_ctrl_get_fd(ctrl);

        asyncIo.WatchFile(ctrl_fd,
                          [this](AsyncIo::EventData eventData) {
                              cvRecv.Notify([this, &eventData]() {
                                  recvReady = eventData.readReady;
                              });
                          });

        while (true)
        {
            size_t len = sizeof(buffer) - 1;
            int retval = wpa_ctrl_recv(ctrl, buffer, &len);
            if (retval < 0)
            {
                if (retval == -2)
                {

                    continue;
                }
                if (errno == EAGAIN)
                {
                    co_await cvRecv.Wait([this]() {
                        if (this->recvAbort)
                        {
                            throw CoQueueClosedException();
                        }
                        if (this->recvReady)
                        {
                            this->recvReady = false;
                            return true;
                        }
                        return false;
                    });
                    continue;
                }
                else
                {
                    throw CoIoException(errno, SS("Failed to receive from " << interfaceName));
                }
            }
            buffer[len] = 0;

            if (traceMessages)
            {
                std::string message{buffer, len};
                if (!message.starts_with("<3>CTRL-EVENT-SCAN-STARTED")) // just too much noise!
                {
                    Log().Info(SS(logPrefix << ":p: " + message));
                }
            }
            WpaEvent *evt = new WpaEvent();
            if (evt->ParseLine(buffer))
            {
                try
                {
                    co_await eventMessageQueue.Push(evt);
                    evt = nullptr;
                }
                catch (const std::exception &e)
                {
                    delete evt;
                    throw;
                }
                if (Dispatcher().IsForeground()) // don't think this happens. but be safe.
                {
                    throw logic_error("Event queue overflowed."); // can only happen if we got suspended trying to push.
                }
            }
            else
            {
                delete evt;
            }
        }
    }
    catch (const std::exception &e)
    {
        if (!this->recvAbort)
        {
            Log().Error(SS(interfaceName << ":p: " << e.what()));
        }
    }
    if (ctrl_fd != -1)
    {
        AsyncIo::GetInstance().UnwatchFile(ctrl_fd);
    }
    if (ctrl != nullptr)
    {
        wpa_ctrl_close(ctrl);
    }
    eventMessageQueue.Close();
    cvRecvRunning.Notify([this]() {
        --this->recvThreadCount;
    });
    co_return;
}
void WpaChannel::CloseChannel(wpa_ctrl *ctrl)
{
    if (ctrl != nullptr)
    {
        wpa_ctrl_close(ctrl);
    }
}

wpa_ctrl *WpaChannel::OpenWpaSocket(const std::string &interfaceName)
{
    std::filesystem::path socketDir{WPA_CONTROL_SOCKET_DIR};

    std::filesystem::path p2pPath = socketDir / interfaceName;

    wpa_ctrl *ctrl{wpa_ctrl_open(p2pPath.c_str())};

    if (!ctrl)
    {
        throw WpaIoException(errno, SS("Can't connect to " << interfaceName));
    }
    return ctrl;
}

bool WpaChannel::IsDisconnected()
{
    return cvDelay.Test<bool>([this]() -> bool {
        return disconnected;
    });
}

CoTask<> WpaChannel::Delay(std::chrono::milliseconds time)
{
    try
    {
        co_await cvDelay.Wait(time,
                              [this]() {
                                  if (this->disconnected)
                                  {
                                      throw WpaDisconnectedException();
                                  }
                                  return false;
                              });
    }
    catch (const CoTimedOutException &)
    {
        co_return;
    }
    co_return; // never actually gets here.
}

CoTask<> WpaChannel::ForegroundEventHandler()
{
    try
    {
        while (true)
        {
            WpaEvent *event = co_await eventMessageQueue.Take();
            OnEvent(*event);
            delete event;
        }
    }
    catch (const CoQueueClosedException &)
    {
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Forground event handler terminated abnormally. " << e.what()));
    }
    cvRecvRunning.Notify([this]() {
        --this->recvThreadCount;
    });

    co_return;
}

void WpaChannel::Ping()
{
    auto response = Request("PING\n");
    if (response.size() != 1 || response[0] != "PONG")
    {
        throw WpaIoException(EBADMSG, "Invalid PING response.");
    }
}

CoTask<> WpaChannel::JoinRecvThread()
{
    if (cvRecvRunning.Test<bool>([this]() { return recvThreadCount == 0; }))
        co_return;
    AbortRecv();
    co_await cvRecvRunning.Wait([this]() {
        return recvThreadCount == 0;
    });
    co_return;
}

WpaChannel::WpaChannel()
{
    SetLog(std::make_shared<ConsoleLog>());
}

std::string WpaChannel::RequestString(const std::string &request, bool throwIfFailed)
{
    auto result = Request(request);
    if (result.size() == 1)
    {
        if (throwIfFailed && (result[0] == "FAIL" || result[0] == "INVALID RESPONSE"))
        {
            throw WpaFailedException(result[0], request);
        }
        return result[0];
    }

    throw WpaFailedException("Wrong size", request);
}
