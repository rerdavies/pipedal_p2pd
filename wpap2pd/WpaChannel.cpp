#include "includes/WpaChannel.h"
#include "ss.h"
#include "cotask/Os.h"
#include <string.h>
#include "includes/P2pUtil.h"

using namespace p2p;
using namespace std;
using namespace cotask;

CoTask<> WpaChannel::RequestOK(const std::string message)
{
    auto response = co_await Request(message);
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
    co_return;
}

CoTask<std::vector<std::string>> WpaChannel::Request(
    const std::string message)
{

    CoLockGuard lock;
    co_await lock.CoLock(requestMutex);

    if (message.size() == 0 || message[message.length() - 1] != '\n')
    {
        throw invalid_argument("Message must end with '\\n'");
    }
    if (traceMessages)
    {
        Log().Info(SS(logPrefix << "> " << message.substr(0, message.length() - 1)));
    }
    size_t len = co_await commandSocket.CoRequest(
        message.c_str(), message.length() - 1,
        requestReplyBuffer, sizeof(requestReplyBuffer));
    requestReplyBuffer[len] = 0;

    vector<string> result;
    const char *p = requestReplyBuffer;
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
    co_return result;
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
            eventSocket.Close();
            eventMessageQueue.Close();

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
                    // should *never* throw.
                    Terminate(" Unexpected exception while terminating recv thread. ");
                }
            }
            commandSocket.Close();
            // once more to make sure all the Disconnect and close exceptions have settled.
            Dispatcher().PumpMessages();
        }
        catch (const std::exception &e)
        {

            std::string message = SS("Exception while closing channel. Terminating.");
            Log().Error(message);

            //~ is noexcept! Any any error here will result in cleanup problems.
            Terminate(message);
        }
    }
    else
    {
        commandSocket.Close();

        Dispatcher().PumpMessages();
    }
}

CoTask<> WpaChannel::OpenChannel(const std::string &interfaceName, bool withEvents)
{
    this->withEvents = withEvents;
    if (withEvents)
    {
        this->interfaceName = interfaceName;

        // seems to be a race condition between receiving group added event and avaialability of the socket.

        for (int retry = 0; true; ++retry)
        {
            try
            {
                eventSocket.Open(interfaceName);
                break;
            }
            catch (const WpaIoException &e)
            {
                if (retry == 5)
                {
                    throw WpaIoException(e.errNo(), SS("Can't open event socket. " << e.what()));
                }
            }
            co_await CoDelay(100ms); // wait and try again.
        }
        co_await eventSocket.Attach();

        closed = false;

        try
        {
            this->commandSocket.Open(interfaceName);
        }
        catch (const WpaIoException &e)
        {
            throw WpaIoException(e.errNo(), SS("Can't open command socket. " << e.what()));
        }

        cvRecvRunning.Execute([this]() {
            this->recvThreadCount = 2;
        });
        Dispatcher().StartThread(ReadEventsProc(interfaceName));

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
                    this->commandSocket.Open(interfaceName);
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
        }
        catch (const WpaIoException &e)
        {
            throw WpaIoException(e.errNo(), SS("Can't open command socket. " << e.what()));
        }
    }
    OnChannelOpened();
}

CoTask<> WpaChannel::ReadEventsProc(std::string interfaceName)
{
    co_await CoBackground();

    try
    {
        char buffer[4096];

        while (true)
        {

            size_t len = co_await eventSocket.CoRecv(buffer, sizeof(buffer));

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

    // make sure both are closed, no matter which one we exited on.
    eventMessageQueue.Close();
    eventSocket.Close();

    cvRecvRunning.Notify([this]() {
        --this->recvThreadCount;
    });
    co_return;
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
            co_await OnEvent(*event);
            delete event;
        }
    }
    catch (const CoIoClosedException &)
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

CoTask<> WpaChannel::Ping()
{
    auto response = co_await Request("PING\n");
    if (response.size() != 1 || response[0] != "PONG")
    {
        throw WpaIoException(EBADMSG, "Invalid PING response.");
    }
    co_return;
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

CoTask<std::string> WpaChannel::RequestString(const std::string request, bool throwIfFailed)
{
    auto result = co_await Request(request);
    if (result.size() == 1)
    {
        if (throwIfFailed && (result[0] == "FAIL" || result[0] == "INVALID RESPONSE"))
        {
            throw WpaFailedException(result[0], request);
        }
        co_return result[0];
    }

    throw WpaFailedException("Wrong size", request);
}

CoTask<std::vector<StationInfo>> WpaChannel::ListSta()
{
    CoLockGuard lock;
    co_await lock.CoLock(requestMutex);

    if (traceMessages)
    {
        Log().Info(SS(logPrefix << "> ListSta"));
    }

    std::vector<StationInfo> result;
    std::string cmd = "STA-FIRST";
    size_t len = co_await commandSocket.CoRequest(
        cmd.c_str(), cmd.length(),
        this->requestReplyBuffer, sizeof(this->requestReplyBuffer) - 1);
    requestReplyBuffer[len] = '\0';

    while (true)
    {
        if (len == 0)
            break;

        if (strcmp(requestReplyBuffer, "FAIL\n") == 0)
        {
            Log().Debug(SS(logPrefix << " ListSta() failed."));
            co_return result;
        }
        if (strcmp(requestReplyBuffer, "UNKNOWN COMMAND\n") == 0)
        {
            Log().Error(SS(logPrefix << " ListSta(): UNKNOWN COMMAND"));
            throw logic_error("ListSta(): UNKNOWN COMMAND");
        }

        StationInfo stationInfo{requestReplyBuffer};
        std::string nextRequest = SS("STA-NEXT " << stationInfo.address);

        result.push_back(std::move(stationInfo));

        len = co_await commandSocket.CoRequest(
            nextRequest.c_str(), nextRequest.length(),
            this->requestReplyBuffer, sizeof(this->requestReplyBuffer) - 1);
        requestReplyBuffer[len] = '\0';
    }
    if (traceMessages)
    {
        if (result.size() == 0)
        {
            Log().Info(SS(logPrefix << "< "));
        }
        else
        {
            for (const StationInfo &stationInfo : result)
            {
                Log().Info(SS(logPrefix << "< " << stationInfo.ToString()));
            }
        }
    }
    co_return result;
}

StationInfo::StationInfo(const char *buffer)
{
    Parse(buffer);
    InitVariables();
}

std::string StationInfo::GetNamedParameter(const std::string &key) const
{
    for (const auto &i : namedParameters)
    {
        if (i.first == key)
        {
            return i.second;
        }
    }
    return "";
}

size_t StationInfo::GetSizeT(const std::string &key) const
{
    auto val = GetNamedParameter(key);
    try
    {
        return ToInt<size_t>(val);
    }
    catch (const std::exception & /*ignored*/)
    {
        return 0;
    }
}
void StationInfo::InitVariables()
{
    // a2:15:e5:0d:91:b2
    // flags=[AUTH][ASSOC][AUTHORIZED]
    // aid=0
    // capability=0x0
    // listen_interval=0
    // supported_rates=0c 18 30
    // timeout_next=NULLFUNC POLL
    // dot11RSNAStatsSTAAddress=a2:15:e5:0d:91:b2
    // dot11RSNAStatsVersion=1
    // dot11RSNAStatsSelectedPairwiseCipher=00-0f-ac-4
    // dot11RSNAStatsTKIPLocalMICFailures=0
    // dot11RSNAStatsTKIPRemoteMICFailures=0
    // wpa=2
    // AKMSuiteSelector=00-0f-ac-2
    // hostapdWPAPTKState=11
    // hostapdWPAPTKGroupState=0
    // p2p_dev_capab=0x27
    // p2p_group_capab=0x0
    // p2p_primary_device_type=10-0050F204-5
    // p2p_device_name=Android_rr
    // p2p_device_addr=ca:74:fa:63:67:58
    // p2p_config_methods=0x188
    // rx_packets=1014
    // tx_packets=2054
    // rx_bytes=42198
    // tx_bytes=313497
    // inactive_msec=9000
    // signal=0
    // rx_rate_info=60
    // tx_rate_info=60
    // connected_time=7282
    // upp_op_classes=51515354737475767778797a7b7c7d7e7f8082

    // just the variables of particular interest.
    address = GetParameter(0);
    p2p_device_name = GetNamedParameter("p2p_device_name");

    rx_bytes = GetSizeT("rx_bytes");
    tx_bytes = GetSizeT("tx_bytes");
    rx_packets = GetSizeT("rx_packets");
    tx_packets = GetSizeT("tx_packets");
}

std::string StationInfo::GetParameter(size_t index) const
{
    if (index >= parameters.size())
        return "";
    return parameters[index];
}

void StationInfo::Parse(const char *buffer)
{

    const char *start;
    start = buffer;
    char c;

    while (*buffer != 0)
    {
        start = buffer;
        const char *equals = nullptr;

        while ((c = *buffer) != '\0' && c != '\n')
        {
            if (c == '=')
            {
                equals = buffer;
            }
            ++buffer;
        }
        if (equals != nullptr)
        {
            namedParameters.push_back(
                std::pair<std::string, std::string>(
                    std::string(start, equals),
                    std::string(equals + 1, buffer)));
        }
        else
        {
            parameters.push_back(std::string(start, buffer));
        }
        if (*buffer != 0)
            ++buffer;
    }
}
std::string StationInfo::ToString() const
{
    return SS(this->p2p_device_name << "(" << this->address << ")");
}
