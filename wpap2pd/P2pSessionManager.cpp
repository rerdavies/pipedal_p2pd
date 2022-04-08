#include "includes/P2pSessionManager.h"
#include "includes/P2pConfiguration.h"
#include "includes/P2pUtil.h"
#include "cotask/Log.h"
#include "ss.h"
#include <cassert>

using namespace p2p;
using namespace std;
using namespace cotask;

class PasswordRemovingLogWrapper : public ILog
{
public:
    using base = ILog;
    PasswordRemovingLogWrapper(std::shared_ptr<ILog> log)
        : innerLog(log), pLog(log.get())
    {
        base::SetLogLevel(log->GetLogLevel());
    }

    virtual void SetLogLevel(LogLevel logLevel) {
         pLog->SetLogLevel(logLevel); 
         base::SetLogLevel(logLevel);

    }

    virtual LogLevel GetLogLevel() const { return pLog->GetLogLevel(); }

protected:
    virtual void OnDebug(const std::string &message) { pLog->Debug(RemovePin(message)); }
    virtual void OnInfo(const std::string &message) { pLog->Info(RemovePin(message)); }
    virtual void OnWarning(const std::string &message) { pLog->Warning(RemovePin(message)); }
    virtual void OnError(const std::string &message) { pLog->Error(RemovePin(message)); }

private:
    std::string RemovePin(const std::string &message)
    {
        auto nPos = message.find(gP2pConfiguration.p2p_pin);
        if (nPos == std::string::npos)
            return message;

        return message.substr(0, nPos) + "********" + message.substr(nPos + gP2pConfiguration.p2p_pin.length());
    }
    std::shared_ptr<ILog> innerLog; // for lifetime.
    ILog *pLog;                     // thread-safe.
};

void P2pSessionManager::SetLog(std::shared_ptr<ILog> log)
{
    std::shared_ptr<ILog> wrapper{new PasswordRemovingLogWrapper(log)};
    base::SetLog(wrapper);
}

void P2pSessionManager::OnEvent(const WpaEvent &event)
{
    base::OnEvent(event);
    try
    {
        switch (event.message)
        {
        case WpaEventMessage::P2P_EVENT_PROV_DISC_PBC_REQ:
            OnProvDiscPbcReq(event);
            break;
        case WpaEventMessage::P2P_EVENT_PROV_DISC_SHOW_PIN:
            OnProvDiscShowPin(event);
            break;
        case WpaEventMessage::P2P_EVENT_GROUP_STARTED:
        {
            auto newGroup = OnGroupStarted(event);
            if (newGroup)
            {
                activeGroups.push_back(std::move(newGroup));
                Log().Info("P2P group available.");

                PreAuthorize(); // set up wpa_supplicant for the *next* connect attempt.
             
            }
            ListNetworks();
        }
        break;

        case WpaEventMessage::P2P_EVENT_GROUP_REMOVED:
            OnGroupRemoved(event);
            break;
        case WpaEventMessage::P2P_EVENT_GO_NEG_REQUEST:
            Log().Debug("P2P_EVENT_GO_NEG_REQUEST");
            OnP2pGoNegRequest(P2pGoNegRequest(event));
            break;
        case WpaEventMessage::P2P_EVENT_DEVICE_FOUND:

            Log().Debug("OnDeviceFound");
            OnDeviceFound(P2pDeviceInfo(event));
            break;
        case WpaEventMessage::P2P_EVENT_DEVICE_LOST:
            OnDeviceLost(event.GetNamedParameter("p2p_dev_addr"));
            break;
        case WpaEventMessage::WPA_EVENT_SCAN_RESULTS:
            // this->scanResults = this->ScanResults();
            break;
        case WpaEventMessage::WPA_EVENT_SCAN_STARTED:
            break;
        case WpaEventMessage::P2P_EVENT_FIND_STOPPED:
            break;
        case WpaEventMessage::WPS_EVENT_TIMEOUT:
            Log().Debug("WPS_EVENT_TIMEOUT");
            this->PreAuthorize();
            break;
        default:
            if (TraceMessages())
            {
                Log().Info(SS("Unhandled: " << event.ToString()));
            }
            break;
        }
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Failed to process event. (" << e.what() << ") " << event.ToString()));
    }
    this->sychronousMessageToWaitFor = WpaEventMessage::WPA_INVALID_MESSAGE;
}

void P2pSessionManager::OnDeviceLost(std::string p2p_dev_addr)
{
    for (auto i = devices.begin(); i != devices.end(); ++i)
    {
        if (i->p2p_dev_addr == p2p_dev_addr)
        {
            devices.erase(i);
            OnDevicesChanged();
            return;
        }
    }
}
void P2pSessionManager::OnDeviceFound(P2pDeviceInfo &&device)
{
    for (auto i = devices.begin(); i != devices.end(); ++i)
    {
        if (i->p2p_dev_addr == device.p2p_dev_addr)
        {
            (*i) = std::move(device);
            OnDevicesChanged();
            return;
        }
    }
    devices.push_back(std::move(device));
    OnDevicesChanged();
}

CoTask<> P2pSessionManager::CoOnUnInit()
{
    co_await base::CoOnUnInit();

    StopServiceDiscovery();

    // Shut down all active groups.
    for (auto &i : activeGroups)
    {
        // 
        Request(SS("P2P_GROUP_REMOVE " << i->GroupInfo().interface << '\n' ));
    }
    // wait for REMOVE_GROUP events to get processed.
    while (activeGroups.size() != 0)
    {
        Dispatcher().PumpMessages(true);
    }
    // Make sure the event queue has drained.
    Dispatcher().PumpMessages();

    co_return;
}
CoTask<> P2pSessionManager::CoOnInit()
{
    co_await base::CoOnInit();
    InitWpaConfig();

    CoDispatcher::CurrentDispatcher().StartThread(KeepAliveProc());
    CoDispatcher::CurrentDispatcher().StartThread(ScanProc());

    this->networkId = FindNetwork();

    SetUpPersistentGroup();

    StartServiceDiscovery();

    co_return;
}

P2pDeviceInfo::P2pDeviceInfo(const WpaEvent &event)
{
    //<3>P2P-DEVICE-FOUND 94:e9:79:05:bc:c9 p2p_dev_addr=96:e9:79:05:bc:c7
    // pri_dev_type=6-0050F204-1 name='DIRECT-roku-880-668333'
    // config_methods=0x80 dev_capab=0x25 group_capab=0x2b
    // wfd_dev_info=0x00111c440096 vendor_elems=1 new=0
    try
    {
        if (event.parameters.size() < 1)
        {
            throw invalid_argument("No device address.");
        }
        //<3>P2P-DEVICE-FOUND 66:0f:86:be:6a:56
        //p2p_dev_addr=66:0f:86:be:6a:56 pri_dev_type=10-0050F204-5
        //name='Android_8d64' config_methods=0x188
        //dev_capab=0x25 group_capab=0x0 new=1
        address = event.parameters[0];
        p2p_dev_addr = event.GetNamedParameter("p2p_dev_addr");
        pri_dev_type = event.GetNamedParameter("pri_dev_type");
        name = WpaEvent::UnquoteString(event.GetNamedParameter("name"));
        config_methods = event.GetNumericParameter<decltype(config_methods)>("config_methods");
        dev_capab = event.GetNumericParameter<decltype(dev_capab)>("dev_capab");
        group_capab = event.GetNumericParameter<decltype(group_capab)>("group_capab");
        wfd_dev_info = event.GetNumericParameter<decltype(wfd_dev_info)>("wfd_dev_info", 0);
        vendor_elems = event.GetNumericParameter<decltype(vendor_elems)>("vendor_elems", 0);
        new_ = toInt<decltype(new_)>(event.GetNamedParameter("new"));
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-DEVICE-FOUND message. " << e.what()));
    }
}

void P2pSessionManager::MaybeEnableNetwork(const std::vector<WpaNetworkInfo> &networks)
{
    if (networks.size() != 0)
    {
        if (networks[0].IsDisabled())
        {
            if (networks[0].IsP2pPersistent())
            {
                EnableNetwork(0);
            }
        }
    }
}

/*
Service broadcast:

1) Send P2PSERV_DISC_EXTERNAL 1
2) Respond to xxx message.
3) Send P2P_SERV_DISC_RESP
4) send P2P_SERVICE_UPDATE...to update.

P2P-PROV-DISC-SHOW-PIN - display PIN.


P2P_EXT_LISTEN - extended listen timing.

CTRL-EVENT-BSS-ADDED event for new BSSs. WPS_AP_AVAILABLE.

*/

//<3>P2P-GO-NEG-REQUEST 8a:3b:2d:b6:9f:8e dev_passwd_id=1 go_intent=14
P2pGoNegRequest::P2pGoNegRequest(const WpaEvent &event)
{
    try
    {
        if (event.parameters.size() < 1)
            throw invalid_argument("No device address.");

        this->src = event.parameters[0];

        this->dev_passwd_id = toInt<decltype(dev_passwd_id)>(event.GetNamedParameter("dev_passwd_id"));
        this->go_intent = toInt<decltype(go_intent)>(event.GetNamedParameter("go_intent"));
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-GO-NEG-REQUEST. " << e.what()));
    }
}

void P2pSessionManager::OnP2pGoNegRequest(const P2pGoNegRequest &request)
{
    return;

    std::string persistent;

    auto config = gP2pConfiguration;

    if (config.persistent_reconnect)
    {
        if (this->networkId != -1)
        {
            persistent = SS(" persistent=" << networkId << " join");
        }
        else
        {
            persistent = " persistent";
        }
    }
    persistent = " persistent";
    std::string configMethod = gP2pConfiguration.p2p_config_method;
    if (configMethod == "none")
    {
        configMethod = "pbc";
    }
    if (configMethod == "keypad")
    {
        configMethod = SS(config.p2p_pin << " keypad");
    }
    std::string pin = gP2pConfiguration.p2p_pin;

    std::string rq = SS(
        "P2P_CONNECT "
        << request.src
        << " " << configMethod
        << persistent
        << " go_intent=" << config.p2p_go_intent
        << (config.p2p_go_ht40 ? " ht40" : "")
        << (config.p2p_go_vht ? " vht" : "")
        << (config.p2p_go_he ? " he" : "")
        << '\n');
    RequestOK(rq);
}

void P2pSessionManager::InitWpaConfig()
{
    SetWpaProperty("device_name", (gP2pConfiguration.p2p_device_name));
    //SetWpaProperty("country", gP2pConfiguration.country_code);
    SetWpaProperty("device_type", (gP2pConfiguration.p2p_device_type));
    SetWpaProperty("persistent_reconnect", gP2pConfiguration.persistent_reconnect ? "1" : "0");
    SetWpaProperty("p2p_go_ht40", gP2pConfiguration.p2p_go_ht40 ? "1" : "0");

    SetWpaProperty("update_config", "1");

    //UdateWpaConfig();
    auto &config = gP2pConfiguration;

    if (config.p2p_config_method == "none")
    {
        SetWpaProperty("config_methods", "pbc");
    }
    else
    {
        SetWpaProperty("config_methods", config.p2p_config_method);
    }

    SetWpaProperty("model_name", config.model_name);
    if (config.manufacturer != "")
        SetWpaProperty("model_number", (config.model_number));
    if (config.model_number != "")
        SetWpaProperty("manufacturer", (config.manufacturer));
    if (config.p2p_serial_number != "")
        SetWpaProperty("serial_number", (config.p2p_serial_number));
    if (config.p2p_sec_device_type != "")
    {
        SetWpaProperty("sec_device_type", config.p2p_sec_device_type);
    }
    if (config.p2p_os_version != "")
        SetWpaProperty("os_version", config.p2p_os_version);

    SetP2pProperty("ssid_postfix", config.p2p_ssid_postfix);
    SetP2pProperty("per_sta_psk", config.p2p_per_sta_psk ? "1" : "0");
}

// static void SetNetworkValue(WpaChannel &channel, ssize_t networkId, const std::string &key, const std::string &value)
// {
//     channel.RequestOK(SS("SET_NETWORK " << networkId << " " << key << " " << value << "\n"));
// }

// static void MaybeSetNetworkValue(WpaChannel &channel, ssize_t networkId, const std::string &key, const std::string &value)
// {
//     std::string existingValue = channel.RequestString(SS("GET_NETWORK " << networkId << " " << key << "\n"), false);
//     if (existingValue != value)
//     {
//         channel.RequestOK(SS("SET_NETWORK " << networkId << " " << key << " " << value << "\n"));
//     }
// }
// static std::vector<WpaNetworkInfo> WlanListNetworks(WpaChannel &channel)
// {
//     auto response = channel.Request("LIST_NETWORKS\n");
//     if (response.size() == 1 && response[0] == "FAIL")
//     {
//         throw WpaIoException(EBADMSG, "Can't enumerate networks on wlan socket.");
//     }
//     std::vector<WpaNetworkInfo> result;
//     for (size_t i = 1; i < response.size(); ++i)
//     {
//         result.push_back(WpaNetworkInfo(response[i]));
//     }
//     return result;
// }

int P2pSessionManager::FindNetwork()
{
    auto networks = ListNetworks();
    std::string suffix = "-" + gP2pConfiguration.p2p_ssid_postfix;
    for (auto &network : networks)
    {
        if (network.Ssid().starts_with("DIRECT-"))
        {
            if (network.Ssid().ends_with(suffix))
            {
                this->networkBsid = network.Bsid();
                return network.Id();
            }
        }
    }
    Log().Debug(SS("Persistent network not found."));
    return -1;

}

void P2pSessionManager::OpenChannel(const std::string &interfaceName, bool withEvents)
{
    base::OpenChannel(interfaceName, withEvents);
}

void P2pSessionManager::SetUpPersistentGroup()
{

    // remove any existing groups.
    auto interfaces = Request("INTERFACES\n");
    for (size_t i = 0; i < interfaces.size(); ++i)
    {
        const std::string &interface = interfaces[i];
        auto nPos = interface.find_first_of('-'); // avoid proccsing wlanX devices.
        if (nPos != std::string::npos)
        {
            if (interface != gP2pConfiguration.p2pInterface) // avoid processing ourself
            {
                try
                {
                    Request(SS("P2P_GROUP_REMOVE " << interface << '\n'));
                    // Succeeded!! Wait for P2P-GROUP-REMOVED message.

                    try
                    {
                        SynchronousWaitForMessage(WpaEventMessage::P2P_EVENT_GROUP_REMOVED, 10000ms);
                    }
                    catch (const CoTimedOutException & /* ignored*/)
                    {
                        Log().Warning("Timed out waiting for P2P_EVENT_GROUP_REMOVED.");
                    }
                }
                catch (const std::exception & /*ignored*/)
                {
                }
            }
        }
    }
    this->addingGroup = true;
    try
    {
        ListNetworks();
        SetP2pProperty("ssid_postfix", "-" + gP2pConfiguration.p2p_ssid_postfix);
        std::string ht40 = gP2pConfiguration.p2p_go_ht40 ? " ht40" : "";
        std::string vht = gP2pConfiguration.p2p_go_vht ? " vht" : "";
        std::string he = gP2pConfiguration.p2p_go_he ? " he" : "";
        //auto networkId = this->networkId;
        SetWpaProperty("p2p_go_intent", WpaEvent::ToIntLiteral(gP2pConfiguration.p2p_go_intent));
        if (this->networkId != -1)
        {
            RequestOK(SS("P2P_GROUP_ADD persistent=" << networkId << " freq=2412 " << ht40 << vht << he << "\n"));
        }
        else
        {
            RequestOK(SS("P2P_GROUP_ADD persistent freq=2412 " << ht40 << vht << he << "\n"));
        }
        SynchronousWaitForMessage(WpaEventMessage::P2P_EVENT_GROUP_STARTED, 10000ms);
        this->addingGroup = false;
    }
    catch (const CoTimedOutException & /* ignored*/)
    {
        this->addingGroup = false;
        Log().Error("Timed out waiting for P2P_EVENT_GROUP_STARTED.");
        SetComplete();
        throw CoIoException(EBADMSG, "Timed out waiting for P2P_EVENT_GROUP_STARTED.");
    }
    catch (const std::exception &e)
    {
        this->addingGroup = false;
        throw CoIoException(EBADMSG, SS("Failed to start Group. " << e.what()));
    }
}

void P2pSessionManager::SynchronousWaitForMessage(WpaEventMessage message, std::chrono::milliseconds timeout)
{
    auto &dispatcher = CoDispatcher::CurrentDispatcher();
    auto endTime = dispatcher.Now() + timeout;
    this->sychronousMessageToWaitFor = message;

    while (true)
    {
        auto delay = endTime - dispatcher.Now();
        if (delay.count() <= 0)
        {
            throw CoTimedOutException();
        }
        dispatcher.PumpMessages(delay);
        if (this->sychronousMessageToWaitFor != message)
        {
            break;
        }
    }
}

std::unique_ptr<P2pGroup> P2pSessionManager::OnGroupStarted(const P2pGroupInfo &groupInfo)
{
    // this->activeGroup = std::make_unique<P2pGroupInfo>(event);

    unique_ptr<P2pGroup> group = std::make_unique<P2pGroup>(this, groupInfo);
    group->SetLog(this->GetSharedLog());
    if (TraceMessages())
    {
        group->TraceMessages(true, "    " + groupInfo.interface);
    }
    this->networkId = FindNetwork();
    group->OpenChannel();

    return group;
}
void P2pSessionManager::OnGroupRemoved(const WpaEvent &event)
{
    // <3>P2P-PROV-DISC-SHOW-PIN d6:fe:1f:53:52:dc 91770561
    //    p2p_dev_addr=d6:fe:1f:53:52:dc
    //    pri_dev_type=10-0050F204-5
    //    name='Android_8d64'
    //    config_methods=0x188
    //    dev_capab=0x25 group_capab=0x0

    // <3>P2P-GROUP-REMOVED p2p-wlan0-13 GO reason=UNAVAILABLE
    if (event.parameters.size() < 1)
        return;
    auto interfaceName = event.parameters[0];
    for (auto i = activeGroups.begin(); i != activeGroups.end(); ++i)
    {
        if ((*i)->GroupInfo().interface == interfaceName)
        {
            activeGroups.erase(i);
            if (activeGroups.size() == 0)
            {
                Log().Info(SS("P2P Group closed (Reason=" << event.GetNamedParameter("reason") << ")"));
                SetComplete();
            }
            break;
        }
    }
}

void P2pSessionManager::SetP2pProperty(const std::string &name, const std::string &value)
{
    auto response = Request(SS("P2P_SET " << name << " " << value << "\n"));
    if (response.size() != 1 || response[0] != "OK")
    {
        Log().Warning(SS("Can't set p2p property " << name));
    }
}

void P2pSessionManager::OnProvDiscPbcReq(const WpaEvent &event)
{
    // <3>P2P-PROV-DISC-PBC-REQ 6a:cd:15:4f:30:33
    //  p2p_dev_addr=6a:cd:15:4f:30:33
    //  pri_dev_type=10-0050F204-5
    //   name='Android_8d64'
    //   config_methods=0x188
    //   dev_capab=0x25 group_capab=0x0
    if (event.parameters.size() < 1)
    {
        throw invalid_argument("Invalid P2P-PROV-DISC-PBC-REQ event.");
    }
    auto &config = gP2pConfiguration;

    if (config.p2p_config_method == "none")
    {
    }
    else if (config.p2p_config_method == "pbc")
    {
        // Implement some way to wait for a PCB here.
        throw logic_error("pbc not implemented.");
    }
    else
    {
        // Implement some way to wait for a PCB here.
        throw logic_error("P2P-PROV-DISC-PBC-REQ not enabled.");
    }
    std::string persistent;
    if (networkId != -1)
    {
        persistent = SS("persistent=" << networkId << " join");
    }
    else
    {
        persistent = "persistent";
    }
    Request("WPS_PBC\n");
    if (this->activeGroups.size() != 0)
    {
        // this->activeGroups[0]->Request("WPS_PBC\n");
    }
    std::string command = SS("P2P_CONNECT "
                             << event.parameters[0]
                             << " pbc"
                             << " " << persistent
                             << " go_intent=" << config.p2p_go_intent
                             << (config.p2p_go_ht40 ? " ht40" : "")
                             << (config.p2p_go_vht ? " vht" : "")
                             << (config.p2p_go_he ? " he" : "")
                             << '\n');
    RequestOK(command);
}
void P2pSessionManager::OnProvDiscShowPin(const WpaEvent &event)
{
    // <3>P2P-PROV-DISC-SHOW-PIN d6:fe:1f:53:52:dc 91770561
    //    p2p_dev_addr=d6:fe:1f:53:52:dc
    //    pri_dev_type=10-0050F204-5
    //    name='Android_8d64'
    //    config_methods=0x188
    //    dev_capab=0x25 group_capab=0x0
    if (event.parameters.size() < 1)
    {
        throw invalid_argument("Invalid P2P-PROV-DISC-SHOW-PIN event.");
    }
    auto &config = gP2pConfiguration;

    if (config.p2p_config_method != "keypad")
    {
        throw logic_error("P2P-PROV-DISC-SHOW-PIN not enabled.");
    }
    networkId = FindNetwork();
    std::string persistent;
    if (networkId != -1)
    {
        persistent = SS("persistent=" << networkId << " join");
    }
    else
    {
        persistent = "persistent";
    }
    PreAuthorize(); // use our pin, not a randomly generated one.

    std::string command = SS("P2P_CONNECT "
                             << event.parameters[0]
                             << " " << config.p2p_pin
                             << " keypad"
                             << " " << persistent
                             << " go_intent=" << config.p2p_go_intent
                             << (config.p2p_go_ht40 ? " ht40" : "")
                             << (config.p2p_go_vht ? " vht" : "")
                             << (config.p2p_go_he ? " he" : "")
                             << '\n');
    std::string pin = RequestString(command);
}

void P2pSessionManager::PreAuthorize()
{
    auto &config = gP2pConfiguration;
    if (config.p2p_config_method == "none")
    {
        RequestOK("WPS_PBC\n"); // PBC is enabled by default!
    }
    else if (config.p2p_config_method == "keypad")
    {
        std::string bsid = networkBsid;
        if (bsid == "any")
        {
            throw logic_error("Invalid network bsid.");
        }



        // if our network has a bssid assigned, used it, otherwise use "any" - which generates a one-time password.
        std::string request = SS("WPS_REG " << bsid << " " << config.p2p_pin << '\n');

        RequestOK(request);

    }
}

P2pSessionManager::P2pSessionManager()
{
    // set the default log to a *wrapped* log.
    auto log = std::make_shared<ConsoleLog>();
    SetLog(log);
}

CoTask<> P2pSessionManager::ScanProc() {
    // We need to nominally keep track of wifi channel usage so that 
    // wpa_supplicant can choose an lightly-used wifi channel.
    // So scan hard initially, and then only occasionally thereafter.

    try {
        P2pFind();
        co_await Delay(60s);
        P2pListen();

        for (int i = 0; i < 10; ++i)
        {
            co_await Delay(60s);
            P2pFind();
            co_await Delay(10s);
            P2pListen();
        }
        while (true)
        {
            co_await Delay(300s);
            P2pFind();
            co_await Delay(15s);
            P2pListen();

        }
    } catch (const std::exception &ignored)
    {
        // we arrive here normally during shutdown.
        // but if there are other issues, here is not the place ot deal with them.
    }
}
CoTask<> P2pSessionManager::KeepAliveProc()
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

static std::string MakeUpnpServiceName()
{
    return SS("uuid:" << gP2pConfiguration.upnpUuid << "::urn:schemas-twoplay-com:service:PiPedal:1");
}

void P2pSessionManager::StopServiceDiscovery()
{
    RequestOK(SS("P2P_SERVICE_DEL upnp 10 " << MakeUpnpServiceName() << '\n'));
}


void P2pSessionManager::StartServiceDiscovery()
{
    // We will will handle our own service discovery requests.

    // RequestOK("P2P_SERVICE_UPDATE\n");

    if (gP2pConfiguration.upnpUuid == "")
    {
        gP2pConfiguration.upnpUuid = os::MakeUuid();
        gP2pConfiguration.Save();
    }
    RequestOK(SS("P2P_SERVICE_ADD upnp 10 " << MakeUpnpServiceName() << '\n'));
}

CoTask<> P2pSessionManager::Open(const std::string &interfaceName)
{
    auto & config = gP2pConfiguration;

    config.wlanInterface = interfaceName;
    config.p2pInterface = "p2p-dev-" + interfaceName;
    co_await base::Open(config.p2pInterface);
    co_return;

}

