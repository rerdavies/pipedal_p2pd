/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "includes/P2pSessionManager.h"
#include "unistd.h"
#include "includes/P2pConfiguration.h"
#include "includes/P2pUtil.h"
#include "cotask/Log.h"
#include "ss.h"
#include <cassert>
#include "cotask/CoExec.h"
#include "includes/WifiP2pDnsSdServiceInfo.h"

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

    virtual void SetLogLevel(LogLevel logLevel)
    {
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

CoTask<> P2pSessionManager::EndEnrollment()
{

    {
        std::lock_guard lock { this->mxEnrollmentRecord};
        this->enrollmentRecord.pin = "";
        this->enrollmentRecord.deviceId = "";
        this->enrollmentRecord.pbc = false;
        this->enrollmentRecord.active = false;
    }

    if (connectedStations != 0)
    {
        co_await P2pStopFind(); // go into passive mode.
    } else {
        co_await P2pFind(15);
    }
    co_return;

}


CoTask<> P2pSessionManager::UpdateStationCount()
{
    if (this->activeGroups.size() > 0)
    {
        try {
            auto stations =  co_await this->activeGroups[0]->ListSta();
            connectedStations = stations.size();
        } catch (const std::exception &e)
        {
            connectedStations = 0;
        }
    } else {
        connectedStations = 0;
    }

}


CoTask<> P2pSessionManager::OnEvent(const WpaEvent &event)
{
    //<3>WPS-ENROLLEE-SEEN 5a:29:ed:1d:f4:d7 129955ef-9dfe-538b-98ee-83315d72526a 0-00000000-0 0x3148 0 1 [ ]
    //<3>WPS-ENROLLEE-SEEN 66:42:08:05:8b:4b 129955ef-9dfe-538b-98ee-83315d72526a 0-00000000-0 0x3148 0 0 [ ]
    //<3>WPS-ENROLLEE-SEEN 16:1b:6f:96:1e:61 129955ef-9dfe-538b-98ee-83315d72526a 0-00000000-0 0x3148 0 1 [ ]
    // <3>AP-STA-DISCONNECTED aa:a3:f2:67:3d:46 p2p_dev_addr=6e:00:18:2b:3b:ac
    // <3>AP-STA-CONNECTED 26:ad:4e:b3:83:30 p2p_dev_addr=d2:fa:ed:2f:43:8d
    // p2p-wlan0-1:p: <3>WPS-ENROLLEE-SEEN aa:a3:f2:67:3d:46 129955ef-9dfe-538b-98ee-83315d72526a 0-00000000-0 0x3148 0 0 [ ]:10:50:11 p2p_dev_addr=6e:00:18:2b:3b:ac
    //<3>AP-STA-DISCONNECTED d6:a5:7a:10:50:11 p2p_dev_addr=6e:00:18:2b:3b:ac
    co_await base::OnEvent(event);
    try
    {
        switch (event.message)
        {
            // <3>CTRL-EVENT-TERMINATING
        case WpaEventMessage::WPA_EVENT_TERMINATING:
        {
            Log().Info("wpa_supplicant terminating. (WPA_EVENT_TERMINATING)");
            SetFinished();
            break;
        }
        break;
        case WpaEventMessage::WPS_EVENT_FAIL:
        {
            Log().Debug(SS("Enrollment failed." << event.ToString()));
            co_await EndEnrollment();
        } break;
        case WpaEventMessage::AP_STA_CONNECTED:
        {

            // <3>AP-STA-CONNECTED 26:ad:4e:b3:83:30 p2p_dev_addr=d2:fa:ed:2f:43:8d
            Log().Info(SS("Station connected: " << bsidToNameMap[event.GetNamedParameter("p2p_dev_addr")] << " " << event.getParameter(0)));

            co_await UpdateStationCount();
            co_await EndEnrollment();

        }
        break;
        case WpaEventMessage::AP_STA_DISCONNECTED:
        {
            // <3>AP-STA-DISCONNECTED d6:a5:7a:10:50:11 p2p_dev_addr=6e:00:18:2b:3b:ac
            Log().Info(SS("Station disconnected: " << event.getParameter(0)));

            co_await UpdateStationCount();
            co_await P2pStopFind();
        }
        break;
        case WpaEventMessage::P2P_EVENT_PROV_DISC_PBC_REQ:
        {
            co_await OnProvDiscPbcReq(event);
        }
        break;
        case WpaEventMessage::P2P_EVENT_PROV_DISC_SHOW_PIN:
        {
            co_await OnProvDiscShowPin(event);
        }
        break;
        case WpaEventMessage::P2P_EVENT_GROUP_STARTED:
        {
            co_await OnGroupStarted(event);
        }
        break;

        case WpaEventMessage::P2P_EVENT_GROUP_REMOVED:
        {
            co_await OnGroupRemoved(event);
        }
        break;
        case WpaEventMessage::P2P_EVENT_GO_NEG_REQUEST:
        {
            Log().Debug("P2P_EVENT_GO_NEG_REQUEST");
            co_await OnP2pGoNegRequest(P2pGoNegRequest(event));
        }
        break;
        case WpaEventMessage::P2P_EVENT_DEVICE_FOUND:
        {
            co_await OnDeviceFound(P2pDeviceInfo(event));
        }
        break;
        case WpaEventMessage::P2P_EVENT_DEVICE_LOST:
        {
            co_await OnDeviceLost(event.GetNamedParameter("p2p_dev_addr"));
        }
        break;
        case WpaEventMessage::WPA_EVENT_SCAN_RESULTS:
            // this->scanResults = this->ScanResults();
            break;
        case WpaEventMessage::WPA_EVENT_SCAN_STARTED:
            break;
        case WpaEventMessage::P2P_EVENT_FIND_STOPPED:
            break;
        case WpaEventMessage::WPS_EVENT_TIMEOUT:
            // Log().Debug("WPS_EVENT_TIMEOUT");
            // this->PreAuthorize();
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
    if (this->synchronousEventToWaitFor == event.message)
    {
        this->synchronousEventToWaitFor = WpaEventMessage::WPA_INVALID_MESSAGE;
    }
}

CoTask<> P2pSessionManager::CoOnUnInit()
{
    co_await base::CoOnUnInit();

    co_await StopServiceDiscovery();

#ifdef JUNK
    // Take snapshot of all active groups.
    std::vector<std::string> groups;
    for (auto &i : activeGroups)
    {
        groups.push_back(i->GetInterfaceName());
    }
    for (auto &group : groups)
    {
        co_await Request(SS("P2P_GROUP_REMOVE " << group << '\n'));
        try
        {
            SynchronousWaitForEvent(WpaEventMessage::P2P_EVENT_GROUP_REMOVED, 10000ms);
        }
        catch (const CoTimedOutException &)
        {
            Log().Warning(SS("Timed out removing group " << group));
        }
    }
#endif
    activeGroups.resize(0); // close the connections (but not the groups).

    co_await this->dnsMasqProcess.Stop();

    co_return;
}
CoTask<> P2pSessionManager::CoOnInit()
{
    co_await base::CoOnInit();
    co_await CleanUpNetworks();
    co_await InitWpaConfig();

    CoDispatcher::CurrentDispatcher().StartThread(KeepAliveProc());
    CoDispatcher::CurrentDispatcher().StartThread(ScanProc());

    this->networkId = co_await FindNetwork();

    co_await SetUpPersistentGroup();

    
    co_await StartServiceDiscovery();

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
        new_ = ToInt<decltype(new_)>(event.GetNamedParameter("new"));
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-DEVICE-FOUND message. " << e.what()));
    }
}

CoTask<> P2pSessionManager::MaybeEnableNetwork(const std::vector<WpaNetworkInfo> &networks)
{
    co_return;
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

        this->dev_passwd_id = ToInt<decltype(dev_passwd_id)>(event.GetNamedParameter("dev_passwd_id"));
        this->go_intent = ToInt<decltype(go_intent)>(event.GetNamedParameter("go_intent"));
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-GO-NEG-REQUEST. " << e.what()));
    }
}

CoTask<> P2pSessionManager::OnP2pGoNegRequest(const P2pGoNegRequest &request)
{
    co_return;
}

CoTask<> P2pSessionManager::CleanUpNetworks()
{
    auto networks = co_await ListNetworks();

    std::string prefix = "DIRECT-";
    std::string postfix = "-" + gP2pConfiguration.p2p_ssid_postfix;

    for (auto&network: networks)
    {
        bool keep = false;
        auto name = network.Ssid();
        if (name.starts_with(prefix) && name.ends_with(postfix))
        {
            keep = true;
        }
        if (!keep)
        {
            co_await RequestOK(SS("REMOVE_NETWORK " << network.Id() << "\n"));
        }
    }
}

CoTask<> P2pSessionManager::InitWpaConfig()
{
    co_await SetWpaProperty("device_name", (gP2pConfiguration.p2p_device_name));
    co_await SetWpaProperty("country", gP2pConfiguration.country_code);
    bool t = this->wpaConfigChanged; // device_type always fails for unknown reasons. BUt it does get set.
    co_await SetWpaProperty("device_type", (gP2pConfiguration.p2p_device_type));
    this->wpaConfigChanged = t;
    co_await SetWpaProperty("persistent_reconnect", gP2pConfiguration.persistent_reconnect ? "1" : "0");
    co_await SetWpaProperty("p2p_go_ht40", gP2pConfiguration.p2p_go_ht40 ? "1" : "0");

    co_await SetWpaProperty("update_config", "1");

    //UdateWpaConfig();
    auto &config = gP2pConfiguration;

    if (config.p2p_config_method == "none")
    {
        co_await SetWpaProperty("config_methods", "pbc");
    }
    else if (config.p2p_config_method == "label")
    {
        co_await SetWpaProperty("config_methods", "keypad");
    }
    else
    {
        co_await SetWpaProperty("config_methods", config.p2p_config_method);
    }

    co_await SetWpaProperty("model_name", config.p2p_model_name);
    if (config.p2p_manufacturer != "")
        co_await SetWpaProperty("model_number", (config.p2p_model_number));
    if (config.p2p_model_number != "")
        co_await SetWpaProperty("manufacturer", (config.p2p_manufacturer));
    if (config.p2p_serial_number != "")
        co_await SetWpaProperty("serial_number", (config.p2p_serial_number));
    if (config.p2p_sec_device_type != "")
    {
        co_await SetWpaProperty("sec_device_type", config.p2p_sec_device_type);
    }
    if (config.p2p_os_version != "")
        co_await SetWpaProperty("os_version", config.p2p_os_version);

    co_await SetP2pProperty("ssid_postfix", config.p2p_ssid_postfix);

    if (this->wpaConfigChanged)
    {
        std::vector<std::string> response = co_await Request("SAVE_CONFIG\n");
        if (response.size() == 0 || response[0] != "OK")
        {
            Log().Warning("Failed to  save updates to wpa_supplicant.conf");
        }
        wpaConfigChanged = false;
    }
    co_await SetP2pProperty("per_sta_psk", config.p2p_per_sta_psk ? "1" : "0");
}

CoTask<int> P2pSessionManager::FindNetwork()
{
    auto networks = co_await ListNetworks();
    std::string suffix = "-" + gP2pConfiguration.p2p_ssid_postfix;
    for (auto &network : networks)
    {
        if (network.Ssid().starts_with("DIRECT-"))
        {
            if (network.Ssid().ends_with(suffix))
            {
                this->networkBsid = network.Bsid();
                co_return network.Id();
            }
        }
    }
    Log().Debug(SS("Persistent network not found."));
    co_return -1;
}

CoTask<> P2pSessionManager::OpenChannel(const std::string &interfaceName, bool withEvents)
{
    co_await base::OpenChannel(interfaceName, withEvents);
}

static bool IsP2pGroup(const std::string &group)
{
    std::string prefix = SS("p2p-" << gP2pConfiguration.wlanInterface << "-");
    return group.starts_with(prefix);
}
CoTask<> P2pSessionManager::RemoveExistingGroups()
{

    // remove any existing groups.
    bool groupRemoved = false;
    auto interfaces = co_await Request("INTERFACES\n");
    for (size_t i = 0; i < interfaces.size(); ++i)
    {
        const std::string &interface = interfaces[i];
        if (IsP2pGroup(interface))
        {
            try
            {
                groupRemoved = true;
                Log().Debug(SS("Removing P2p group " << interface));

                co_await Request(SS("P2P_GROUP_REMOVE " << interface << '\n'));
                // Succeeded!! Wait for P2P-GROUP-REMOVED message.

                try
                {
                    SynchronousWaitForEvent(WpaEventMessage::P2P_EVENT_GROUP_REMOVED, 10000ms);
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
    if (groupRemoved)
    {
        // give wpa_supplicant a chance to sort out networks?
        // Whatever the reason, we need to delay in order to prevent getting an UNAVAILABLE error when we try to create our new persistent group.
        Log().Debug("Waiting for networks to settle after a group change.");
        co_await this->Delay(10s);
    }
}
CoTask<> P2pSessionManager::SetUpPersistentGroup()
{
    // Just recycle p2p-wlan0-0.
    // co_await RemoveExistingGroups();

    auto &config = gP2pConfiguration;
    co_await SetP2pProperty("ssid_postfix", "-" + config.p2p_ssid_postfix);
    co_await SetWpaProperty("p2p_go_intent", WpaEvent::ToIntLiteral(config.p2p_go_intent));

    std::vector<std::string> interfaces = co_await Request("INTERFACES\n");
    std::string interfaceName;

    std::string groupPrefix = "p2p-" + config.wlanInterface + "-0";
    for (const std::string &interface : interfaces)
    {
        if (interface.starts_with(groupPrefix))
        {
            interfaceName = interface;
            break;
        }
    }

    if (interfaceName != "")
    {
        co_await CreateP2pGroup(interfaceName);
    }
    else
    {

        this->addingGroup = true;
        try
        {
            co_await ListNetworks();
            std::string ht40 = config.p2p_go_ht40 ? " ht40" : "";
            std::string vht = config.p2p_go_vht ? " vht" : "";
            std::string he = config.p2p_go_he ? " he" : "";

            co_await SetWpaProperty("p2p_go_intent", WpaEvent::ToIntLiteral(config.p2p_go_intent));

            Log().Info(SS("Creating persistent P2p Group."));

            if (this->networkId != -1)
            {
                co_await RequestOK(SS("P2P_GROUP_ADD persistent=" << networkId << " freq=" << config.wifiGroupFrequency << ht40 << vht << he << "\n"));
            }
            else
            {
                co_await RequestOK(SS("P2P_GROUP_ADD persistent freq=" << config.wifiGroupFrequency << ht40 << vht << he << "\n"));
            }
            SynchronousWaitForEvent(WpaEventMessage::P2P_EVENT_GROUP_STARTED, 40000ms);
            Log().Debug(SS("Created persistent P2p Group."));

            if (this->activeGroups.size() == 0)
            {
                std::string message = "Failed to create group p2p-"  + gP2pConfiguration.wlanInterface +"-0";
                Log().Error(message);
                Log().Error("Try 'sudo systemctl restart dhcpcd'.");
                throw logic_error("Failed to create group interface.");
            }

            this->addingGroup = false;
        }
        catch (const CoTimedOutException & /* ignored*/)
        {
            this->addingGroup = false;
            Log().Error("Timed out waiting for P2P_EVENT_GROUP_STARTED.");
            SetFinished();
            throw CoIoException(EBADMSG, "Timed out waiting for P2P_EVENT_GROUP_STARTED.");
        }
        catch (const std::exception &e)
        {
            this->addingGroup = false;
            throw CoIoException(EBADMSG, SS("Failed to start Group. " << e.what()));
        }
    }
    if (gP2pConfiguration.runDnsMasq)
    {
        try {
        this->dnsMasqProcess.Start(this->GetSharedLog(),this->interfaceName);
        co_await this->Delay(100ms);
        if (this->dnsMasqProcess.HasTerminated()) {
            co_await dnsMasqProcess.Stop(); // flush output to log.
            Log().Warning("Failed to start dnsmasq DHCP server.");
        }
        } catch (const std::exception & e)
        {
            Log().Warning(SS("Failed to start dnsmasq DHCP server. " << e.what()));        
        }
    }
    Log().Info("Ready");
}

void P2pSessionManager::SynchronousWaitForEvent(WpaEventMessage message, std::chrono::milliseconds timeout)
{
    auto &dispatcher = CoDispatcher::CurrentDispatcher();
    auto endTime = dispatcher.Now() + timeout;
    this->synchronousEventToWaitFor = message;

    while (true)
    {
        auto delay = endTime - dispatcher.Now();
        if (delay.count() <= 0)
        {
            throw CoTimedOutException();
        }
        dispatcher.PumpMessages(delay);
        if (this->synchronousEventToWaitFor != message)
        {
            break;
        }
    }
}

CoTask<> P2pSessionManager::CreateP2pGroup(const std::string &interfaceName)
{
    try
    {
        unique_ptr<P2pGroup> group = std::make_unique<P2pGroup>(this, interfaceName);
        group->SetLog(this->GetSharedLog());
        if (TraceMessages())
        {
            group->TraceMessages(true, "    " + interfaceName);
        }
        co_await group->OpenChannel();

        activeGroups.push_back(std::move(group));
        Log().Info("P2P group available.");
        this->interfaceName = interfaceName;
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Failed to create P2pGroup for " << interfaceName << ". " << e.what()));
    }
}

CoTask<> P2pSessionManager::AttachDnsMasq()
{
    CoExec coExec;
    // dnsmasq doesn't automatically attach to our new network.
    // To get it to do so, restart the service.

    // prompts for sudo when developing.
    // runs without prompts when running as a daemon.
    Log().Info("Restarting dnsmasq service.");
    std::string output;
    std::vector<std::string> args{ "restart","dnsmasq"};
    bool result = co_await coExec.CoExecute("/usr/bin/systemctl", args,&output);
    if (result != 0)
    {
        Log().Error("Failed to restart dnsmasq service.");
        if (output.length() != 0)
        {
            Log().Error("-- " + output);
        }
    }

    co_return;
}

CoTask<> P2pSessionManager::OnGroupStarted(
    const P2pGroupInfo &groupInfo)
{

    std::string desiredInterface = "p2p-" + gP2pConfiguration.wlanInterface + "-0";
    if (groupInfo.interface != desiredInterface)
    {
        gotWrongInterface = true;
        Log().Error("Unexpected interface added. (Expecting "+ desiredInterface+ "; got " + groupInfo.interface+ ")");
         co_return;
    }
    // this->activeGroup = std::make_unique<P2pGroupInfo>(event);
    co_await CreateP2pGroup(groupInfo.interface);

//    co_await AttachDnsMasq();
    co_return;
}
CoTask<> P2pSessionManager::OnGroupRemoved(const WpaEvent &event)
{
    // <3>P2P-PROV-DISC-SHOW-PIN d6:fe:1f:53:52:dc 91770561
    //    p2p_dev_addr=d6:fe:1f:53:52:dc
    //    pri_dev_type=10-0050F204-5
    //    name='Android_8d64'
    //    config_methods=0x188
    //    dev_capab=0x25 group_capab=0x0

    // <3>P2P-GROUP-REMOVED p2p-wlan0-13 GO reason=UNAVAILABLE
    if (event.parameters.size() < 1)
        co_return;
    auto interfaceName = event.parameters[0];
    for (auto i = activeGroups.begin(); i != activeGroups.end(); ++i)
    {
        if ((*i)->GetInterfaceName() == interfaceName)
        {

            // don't delete while in vector erase.
            std::unique_ptr<P2pGroup> p = std::move(*i);
            activeGroups.erase(i);

            // now delete it.
            p = nullptr;

            if (activeGroups.size() == 0)
            {
                Log().Info(SS("P2P Group closed (Reason=" << event.GetNamedParameter("reason") << ")"));
                SetFinished();
            }
            break;
        }
    }
}

CoTask<> P2pSessionManager::SetP2pProperty(const std::string &name, const std::string &value)
{
    auto response = co_await Request(SS("P2P_SET " << name << " " << value << "\n"));
    if (response.size() != 1 || response[0] != "OK")
    {
        Log().Warning(SS("Can't set p2p property " << name));
    }
    co_return;
}

CoTask<> P2pSessionManager::OnProvDiscPbcReq(const WpaEvent &event)
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
    {
        std::lock_guard lock { this->mxEnrollmentRecord};
        this->enrollmentRecord.pin = "";
        this->enrollmentRecord.deviceId = event.getParameter(0);
        this->enrollmentRecord.pbc = true;
        this->enrollmentRecord.active = true;
    }

    std::string command = SS("P2P_CONNECT "
                             << event.parameters[0]
                             << " pbc"
                             << " " << persistent
                             << " go_intent=" << config.p2p_go_intent
                             << " freq=" << config.wifiGroupFrequency // do NOT frequency hop when connected.
                             << (config.p2p_go_ht40 ? " ht40" : "")
                             << (config.p2p_go_vht ? " vht" : "")
                             << (config.p2p_go_he ? " he" : "")
                             << '\n');
    co_await RequestOK(command);
    co_return;
}
CoTask<> P2pSessionManager::OnProvDiscShowPin(const WpaEvent &event)
{
    // <3>P2P-PROV-DISC-SHOW-PIN d6:fe:1f:53:52:dc 91770561
    //    p2p_dev_addr=d6:fe:1f:53:52:dc
    //    pri_dev_type=10-0050F204-5
    //    name='Android_8d64'
    //    config_methods=0x188
    //    dev_capab=0x25 group_capab=0x0
    if (event.parameters.size() < 2)
    {
        throw invalid_argument("Invalid P2P-PROV-DISC-SHOW-PIN event.");
    }
    auto &config = gP2pConfiguration;

    if (config.p2p_config_method != "keypad" && config.p2p_config_method != "label")
    {
        throw logic_error("P2P-PROV-DISC-SHOW-PIN not enabled.");
    }
    std::string pin;
    if (config.p2p_config_method == "label")
    {
        pin = config.p2p_pin;
    }
    else
    {
        pin = event.getParameter(1);
        Log().Debug("------------------------");
        Log().Debug("     " + pin);
        Log().Debug("------------------------");
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

    {
        std::lock_guard lock { this->mxEnrollmentRecord};
        this->enrollmentRecord.pin = pin;
        this->enrollmentRecord.deviceId = event.getParameter(0);
        this->enrollmentRecord.pbc = false;
        this->enrollmentRecord.active = true;
    }


    std::string command = SS("P2P_CONNECT "
                             << event.getParameter(0)
                             << " " << pin
                             << " " << "keypad"
                             << " " << persistent
                             << " go_intent=" << config.p2p_go_intent
                             << " freq=" << config.wifiGroupFrequency // do NOT frequency hop when connected.
                             << (config.p2p_go_ht40 ? " ht40" : "")
                             << (config.p2p_go_vht ? " vht" : "")
                             << (config.p2p_go_he ? " he" : "")
                             // << " provdisc"
                             << '\n');
    std::string strResult = co_await RequestString(command);
    if (strResult != "OK")
    {
        throw CoIoException(EBADMSG, "P2P_CONNECT FAILED.");
    }
}

P2pSessionManager::P2pSessionManager()
{
    // set the default log to a *wrapped* log.
    auto log = std::make_shared<ConsoleLog>();
    SetLog(log);
}

CoTask<> P2pSessionManager::ScanProc()
{
    // We need to nominally keep track of wifi channel usage so that
    // wpa_supplicant can choose an lightly-used wifi channel.
    // So scan hard initially, and then only occasionally thereafter.
    // XXX: Suspend this while an enrollment is underway!
    // Channel hopping would be bad.
    try
    {
        while (true)
        {
        start:
            while (connectedStations != 0)
            {
                co_await Delay(2s);
            }
            co_await P2pFind();
            co_await Delay(63s);
            if (connectedStations != 0)
                goto start; // REMAIN in Listen mode if we are connected.

            co_await P2pStopFind();

            for (int i = 0; i < 10; ++i)
            {
                co_await Delay(120s);
                if (connectedStations != 0)
                    goto start;

                co_await P2pFind();

                co_await Delay(15s);
                if (connectedStations != 0)
                    goto start;

                co_await P2pStopFind();
            }
            while (true)
            {
                co_await Delay(321s);
                if (connectedStations != 0)
                    goto start;

                co_await P2pFind();
                co_await Delay(15s);
                if (connectedStations != 0)
                    goto start;

                co_await P2pStopFind();
            }
        }
    }
    catch (const std::exception &e)
    {
        // we arrive here normally during shutdown.
        // but if there are other issues, here is not the place ot deal with them.
        Log().Debug(SS("Listen thread terminated. " << e.what()));
    }
}

static std::string MakeUpnpServiceName()
{
    return SS("uuid:" << gP2pConfiguration.service_guid << "::urn:schemas-twoplay-com:service:PiPedal:1" 
    << "::port:" << gP2pConfiguration.server_port
        );
}

CoTask<> P2pSessionManager::StopServiceDiscovery()
{
    try {
        // co_await RequestOK(SS("P2P_SERVICE_DEL upnp 10 " << MakeUpnpServiceName() << '\n'));
        co_await RequestOK("P2P_SERVICE_FLUSH\n"); // reset all outstanding Wi-Fi Direct service announcements.
    } catch (const std::exception&)
    {

    }
}

CoTask<> P2pSessionManager::StartServiceDiscovery()
{
    // We will will handle our own service discovery requests.

    // RequestOK("P2P_SERVICE_UPDATE\n");


    co_await RequestOK("P2P_SERVICE_FLUSH\n"); // reset all outstanding Wi-Fi Direct service announcements.

    if (gP2pConfiguration.service_guid == "")
    {
        gP2pConfiguration.service_guid = os::MakeUuid();
        gP2pConfiguration.Save();
    }
    co_await RequestOK(SS("P2P_SERVICE_ADD upnp 10 " << MakeUpnpServiceName() << '\n'));


    std::vector<std::pair<std::string,std::string>> txtRecords {
        {"id",gP2pConfiguration.service_guid}
    };
#ifdef JUNK
    // not signficiantly better than upnp.
    // TXT records don't work.
    char hostname[512];
    gethostname(hostname,sizeof(hostname)-1);
    hostname[sizeof(hostname)-1] = '\0';

    WifiP2pDnsSdServiceInfo bonjourServiceInfo {
        "UniqueName", // xxx fix me!
        "_pipedal._tcp",
        txtRecords
    };

    for (const std::string&query: bonjourServiceInfo.GetQueryList())
    {
        this->Log().Info(SS("Wifi DNS-SD query: " << query));
        co_await RequestOK(SS("P2P_SERVICE_ADD " << query << '\n'));
    }
#endif
}

CoTask<> P2pSessionManager::Open(const std::string &interfaceName)
{
    auto &config = gP2pConfiguration;

    config.wlanInterface = interfaceName;
    config.p2pInterface = "p2p-dev-" + interfaceName;
    co_await base::Open(config.p2pInterface);
    co_return;
}

CoTask<> P2pSessionManager::OnDeviceFound(P2pDeviceInfo &&device)
{
    //<3>P2P-DEVICE-FOUND b0:a7:37:b3:72:21 p2p_dev_addr=b2:a7:37:b3:72:1f pri_dev_type=6-0050F204-1
    //    name='Roku 3' config_methods=0x0 dev_capab=0x35 group_capab=0x2b
    //    wfd_dev_info=0x01111c440096 vendor_elems=1 new=1
    bsidToNameMap[device.p2p_dev_addr] = device.name;
    co_return;
}
CoTask<> P2pSessionManager::OnDeviceLost(std::string p2p_dev_addr)
{
    bsidToNameMap.erase(p2p_dev_addr);
    co_return;
}


CoTask<> P2pSessionManager::CloseGroup(P2pGroup* group)
{
    Dispatcher().PostDelayedFunction(
        Dispatcher().Now(),
        [this,group] () {
            for (size_t i = 0; i < activeGroups.size(); ++i)
            {
                if (activeGroups[i].get() == group)
                {
                    if (activeGroups[i].get() == group)
                    {
                        Log().Debug(SS("P2P Group closed."));
                        activeGroups.erase(activeGroups.begin()+i); // deletes the group!
                    }
                }
                if (activeGroups.size() == 0)
                {
                    Log().Info("P2P Group closed.");
                    SetFinished();
                }
            }
        });
    co_return;    
}
