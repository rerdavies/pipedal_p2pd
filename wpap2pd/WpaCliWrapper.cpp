#include "includes/WpaCliWrapper.h"
#include "cotask/Os.h"
#include "cotask/CoEvent.h"
#include "cotask/CoExceptions.h"
#include "ss.h"
#include "includes/P2pUtil.h"
#include "includes/P2pConfiguration.h"


extern "C"
{
#include "wpa_ctrl.h"
}

using namespace cotask;
using namespace p2p;
using namespace std;

const char *WPA_CONTROL_SOCKET_DIR = "/var/run/wpa_supplicant";



WpaCliWrapper::~WpaCliWrapper()
{
    OnDisconnected();
    if (recvRunning)
    {
        if (cvRecvRunning.Test<bool>([this]() { return this->recvRunning; }))
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
                Log().Error(SS("Unexpected exception while terminating recv thread. " << e.what()));
                std::terminate();
            }
        }
    }
    // once more to make sure all the Disconnect exceptions have settled.
    Dispatcher().PumpMessages();

    CloseWpaCtrl(this->p2pCommandSocket);
}

CoTask<> WpaCliWrapper::Open(int argc, const char *const *argv)
{
    try
    {
        this->traceMessages = true;

        vector<string> args;

        for (int i = 0; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--")
            {
                args.resize(0);
            }
            else if (arg == "--trace")
            {
                this->traceMessages = true;
            }
            else
            {
                args.push_back(argv[i]);
            }
        }

        {
            std::filesystem::path socketDir{WPA_CONTROL_SOCKET_DIR};

            std::filesystem::path p2pPath = socketDir / p2pInterfaceName;

            wpa_ctrl *ctrl = wpa_ctrl_open(p2pPath.c_str());

            if (!ctrl)
            {
                throw std::logic_error(SS("Can't connect to " << p2pInterfaceName));
            }

            for (int retry = 0; true; ++retry)
            {
                int result = wpa_ctrl_attach(ctrl);
                if (result != 0)
                {
                    if (result == -2)
                    {
                        wpa_ctrl_close(ctrl);
                        if (retry == 3)
                        {
                            auto message = SS("Timed out attaching to " << p2pInterfaceName);
                            throw CoIoException(ETIMEDOUT,message);
                        }
                        auto message = SS("Timed out attaching to " << p2pInterfaceName << ". Retrying...");

                        Log().Warning(message);
                        continue;
                    } else {
                        wpa_ctrl_close(ctrl);
                        throw CoIoException(errno, SS("Failed to attach to " << p2pInterfaceName));
                    }
                }
                break;
            }

            cvRecvRunning.Execute([this]() {
                this->recvRunning = true;
            });
            Dispatcher().StartThread(ReadP2pEventsProc(ctrl, p2pInterfaceName));
        }

        this->p2pCommandSocket = OpenWpaCtrl(p2pInterfaceName);

        Dispatcher().StartThread(ForegroundEventHandler());
        Dispatcher().StartThread(KeepAliveProc());

        co_await CoOnInit();
    }
    catch (const std::exception &e)
    {
        Log().Error(e.what());
    }
    co_return;
}

#ifdef JUNK
CoTask<std::vector<std::string>> WpaCliWrapper::ReadToPrompt(std::chrono::milliseconds timeout)
{
    std::vector<std::string> response;
    while (true)
    {
        while (lineBufferHead != lineBufferTail)
        {
            char c = lineBuffer[lineBufferHead];
            if (c == '\n')
            {
                response.push_back(lineResult.str());
                lineResult.clear();
                lineResultEmpty = true;
            }
            else if (lineResultEmpty && c == '>')
            {
                if (lineBufferRecoveringFromTimeout)
                {
                    lineBufferRecoveringFromTimeout = true;
                    response.resize(0);
                }
                else
                {
                    NotifyPrompting();
                    co_return response;
                }
            }
            else
            {
                lineResultEmpty = false;
                lineResult << c;
            }
        }
        try
        {
            int nRead = co_await Stdin().CoRead(lineBuffer, sizeof(lineBuffer), timeout);
            lineBufferHead = 0;
            lineBufferTail = nRead;
        }
        catch (const CoTimedOutException &e)
        {
            lineBufferRecoveringFromTimeout = true;
            throw;
        }
    }
    co_return response;
}
#endif

void WpaCliWrapper::PostQuit()
{
    Dispatcher().PostQuit();
}

CoTask<> WpaCliWrapper::ReadP2pEventsProc(wpa_ctrl *ctrl, std::string interfaceName)
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
                            throw logic_error("Closed.");
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
                Log().Info("p: " + message);
            }
            WpaEvent *evt = new WpaEvent();
            if (evt->ParseLine(buffer))
            {
                try {
                    co_await p2pControlMessageQueue.Push(evt);
                    evt = nullptr;
                } catch (const std::exception &e)
                {
                    delete evt;
                    throw;
                }
                if (Dispatcher().IsForeground()) // don't think this happens. but be safe.
                {
                    throw logic_error("Event queue overflowed."); // can only happen if we got suspended trying to push.
                }
            } else {
                delete evt;
            }
        }
    }
    catch (const std::exception &e)
    {
        if (!this->recvAbort)
        {
            Log().Error(e.what());
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
    p2pControlMessageQueue.Close();
    cvRecvRunning.Notify([this]() {
        this->recvRunning = false;
    });
    co_return;
}

std::vector<std::string> WpaCliWrapper::Request(const std::string &message)
{
    return Request(this->p2pCommandSocket, message);
}

void WpaCliWrapper::RequestOK(const std::string &message)
{
    auto response = Request(message);
    if (response.size() >= 1) {
        if (response[0] != "OK")
        {
            throw WpaIoException(EBADMSG,SS("Request failed. (" << response[0] << ""));
        }
    } else {
        throw WpaIoException(EBADMSG,SS("Request failed. (No response)."));
    }
}

std::vector<std::string> WpaCliWrapper::Request(
    wpa_ctrl *ctl, 
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
        Log().Info(SS("> " << message.substr(0, message.length() - 1)));
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
        retVal = wpa_ctrl_request(ctl, message.c_str(), message.length() - 1, reply, &len, nullptr);
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
            if (errno != 0) {
                CoIoException::ThrowErrno();
            } else {
                throw WpaIoException(EBADMSG,"Failed.");
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
            Log().Info("< " + line);
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
        if (result[0] == "FAIL")
        {
            throw WpaIoException(EBADMSG, SS("Command failed."));
        }
    }
    return result;
}

void WpaCliWrapper::CloseWpaCtrl(wpa_ctrl *ctrl)
{
    if (ctrl != nullptr)
    {
        wpa_ctrl_close(ctrl);
    }
}
wpa_ctrl *WpaCliWrapper::OpenWpaCtrl(const std::string &interfaceName)
{
    std::filesystem::path socketDir{WPA_CONTROL_SOCKET_DIR};

    std::filesystem::path p2pPath = socketDir / p2pInterfaceName;

    wpa_ctrl *ctrl{wpa_ctrl_open(p2pPath.c_str())};

    if (!ctrl)
    {
        throw std::logic_error(SS("Can't connect to " << p2pInterfaceName));
    }
    return ctrl;
}

void WpaCliWrapper::UpdateStatus()
{
    networks = ListNetworks();
}

WpaNetwork::WpaNetwork(const std::string &wpaResponse)
{
    vector<string> args = split(wpaResponse, '\t');
    if (args.size() < 4)
    {
        throw invalid_argument("Invalid wapResponse");
    }
    this->number = std::stoi(args[0]);

    this->ssid = args[1];

    this->bsid = args[2];

    this->flags = splitWpaFlags(args[3]);
}
std::vector<WpaNetwork> WpaCliWrapper::ListNetworks()
{
    auto response = Request("LIST_NETWORKS\n");

    vector<WpaNetwork> result;
    for (size_t i = 1; i < response.size(); ++i)
    {
        result.push_back(WpaNetwork(response[i]));
    }
    return result;
}

WpaStatus::WpaStatus(const std::vector<std::string> &wpaResponse)
{
    for (auto &i : wpaResponse)
    {
        int pos = i.find_first_of('=');
        std::string tag = i.substr(0, pos);
        std::string value = i.substr(pos + 1);

        if (tag == "bssid")
        {
            this->bssid = value;
        }
        else if (tag == "ssid")
        {
            this->ssid = value;
        }
        else if (tag == "pairwise_cipher")
        {
            this->pairwiseCipher = value;
        }
        else if (tag == "group_cipher")
        {
            this->groupCipher = value;
        }
        else if (tag == "key_mgmt")
        {
            this->keyMgmt = value;
        }
        else if (tag == "wpa_state")
        {
            this->wpaState = value;
        }
        else if (tag == "ip_address")
        {
            ipAddress = value;
        }
        else if (tag == "Supplicant PAE state")
        {
            supplicantPaeState = value;
        }
        else if (tag == "suppPortStatus")
        {
            suppPortStatus = value;
        }
        else if (tag == "EAP state")
        {
            eapState = value;
        }
        else
        {
            extras.push_back(i);
        }
    }
}
WpaStatus WpaCliWrapper::Status()
{
    auto response = Request("STATUS");
    return WpaStatus(response);
}

void WpaCliWrapper::Bssid(int network, std::string bssid)
{
    std::stringstream s;
    s << "BSSID " << network << " " << bssid;

    RequestOK(s.str());
}

std::vector<WpaScanResult> WpaCliWrapper::ScanResults()
{
    auto response = Request("SCAN_RESULTS\n");
    std::vector<WpaScanResult> result;
    for (size_t i = 1; i < response.size(); ++i)
    {
        try
        {
            result.push_back(WpaScanResult(response[i]));
        }
        catch (std::exception &e)
        {
            // truncated result (due to UDP packet size limit?)
        }
    }
    return result;
}

void WpaCliWrapper::OnDisconnected()
{
    cvDelay.NotifyAll([this]() {
        this->disconnected = true;
    });
}

WpaScanResult::WpaScanResult(const std::string &wpaResponse)
{
    std::vector<std::string> args = split(wpaResponse, '\t');
    if (args.size() < 5)
    {
        throw logic_error("Invalid ScanResult response.");
    }
    this->bssid = args[0];
    this->frequency = toInt64(args[1]);
    this->signalLevel = toInt64(args[2]);
    this->flags = splitWpaFlags(args[3]);
    this->ssid = args[4];
}

CoTask<> WpaCliWrapper::JoinRecvThread()
{
    if (!recvRunning)
        co_return;
    AbortRecv();
    co_await cvRecvRunning.Wait([this]() {
        return !recvRunning;
    });
    co_return;
}

void WpaCliWrapper::PreAuth(std::string bssid)
{
    RequestOK(SS("PREAUTH " << bssid << '\n'));
}

void WpaCliWrapper::Level(std::string debugLevel)
{
    RequestOK(SS("LEVEL " << debugLevel << '\n'));
}

void WpaCliWrapper::Ping()
{
    auto response = Request("PING\n");
    if (response.size() != 1 || response[0] != "PONG")
    {
        throw WpaIoException(EBADMSG, "Invalid PING response.");
    }
}

CoTask<> WpaCliWrapper::Delay(std::chrono::milliseconds time)
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

bool WpaCliWrapper::IsDisconnected()
{
    return cvDelay.Test<bool>([this]() -> bool {
        return disconnected;
    });
}

CoTask<> WpaCliWrapper::ForegroundEventHandler()
{
    try
    {
        while (true)
        {
            WpaEvent * event = co_await p2pControlMessageQueue.Take();
            Log().Debug(SS("Message on main thread. " << event->ToString()));
            OnEvent(Source::P2p, *event);
            delete event;
        }
    }
    catch (const CoQueueClosedException &)
    {
    }
    co_return;
}

CoTask<> WpaCliWrapper::KeepAliveProc()
{
    // Ping every 15 seconds, just to make sure wpa_supplicant is responsive.
    try
    {
        Ping();
        co_await this->Delay(15000ms);
    }
    catch (const WpaDisconnectedException &)
    {
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("wpa_supplicant is not responding. " << e.what()));
    }
    co_return;
}
void WpaCliWrapper::P2pFind(int timeoutSeconds)
{
    RequestOK(SS("P2P_FIND " << timeoutSeconds << "\n"));
}
void WpaCliWrapper::P2pFind(int timeoutSeconds, const std::string &type)
{
    RequestOK(SS("P2P_FIND " << timeoutSeconds << " " << type << "\n"));

}

void WpaCliWrapper::EnableNetwork(int networkId)
{
    try {
        RequestOK(SS("ENABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
        auto message = SS("Failed to eable network. " << e.what() );
        throw CoIoException(e.errNo(),message);
    }
}
void WpaCliWrapper::DisableNetwork(int networkId)
{
    try {
        RequestOK(SS("DISABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
    }
}

std::string WpaCliWrapper::RequestString(const std::string &name)
{
    auto result = Request(name);
    if (result[0].size() == 1) return result[0];
    throw CoIoException(EBADMSG,"Invalid response.");
}


std::string WpaCliWrapper::GetWpaProperty(const std::string &name)
{
    try {
    auto result = Request(SS("GET " << name << '\n'));
    if (result.size() == 1)
    {
        return result[0];
    }
    } catch (const std::exception &e)
    {
        Log().Debug(SS("Can't get property" << name));
    }
    return "";
}

void WpaCliWrapper::SetWpaProperty(const std::string&name, const std::string &value)
{
    if (GetWpaProperty(name) != value)
    {
        try {
            RequestOK(SS("SET " << name << " " << value << "\n"));
            wpaConfigChanged = true;
        } catch (const std::exception & e)
        {
            Log().Error(SS("Cant set wpa property " << name << ". " << e.what()));
        }

    }
}
void WpaCliWrapper::SetWpaP2pProperty(const std::string &name, const std::string &value)
{
    RequestOK(SS("P2P_SET " << name << " " << value << "\n"));

}
void WpaCliWrapper::UdateWpaConfig()
{
    if (wpaConfigChanged)
    {
        wpaConfigChanged = false;
        if (gP2pConfiguration.updateConfig)
        {

        }
    }

}


