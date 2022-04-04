#include "includes/P2pSessionManager.h"
#include "includes/P2pConfiguration.h"
#include "includes/P2pUtil.h"
#include "ss.h"
#include <cassert>

using namespace p2p;
using namespace std;

void P2pSessionManager::OnEvent(Source source, WpaEvent &event)
{
    base::OnEvent(source, event);
    if (source == Source::P2p)
    {
        try
        {
            switch (event.message)
            {
            case WpaEventMessage::P2P_EVENT_GROUP_STARTED:
                if (this->addingGroup) // don't accidentally pick up somebody ele's group.
                {
                    OnGroupStarted(event);
                }
                break;

            case WpaEventMessage::P2P_EVENT_GROUP_REMOVED:
                OnGroupStopped(event);
                break;
            case WpaEventMessage::P2P_EVENT_GO_NEG_REQUEST:
                Log().Debug("P2P_EVENT_GO_NEG_REQUEST");
                OnP2pGoNegRequest(P2pGoNegRequest(event));
                break;
            case WpaEventMessage::P2P_EVENT_DEVICE_FOUND:

                Log().Debug(">OnDeviceFound");
                OnDeviceFound(P2pDevice(event));
                Log().Debug("<OnDeviceFound");
                break;
            case WpaEventMessage::P2P_EVENT_DEVICE_LOST:
                OnDeviceLost(event.GetNamedParameter("p2p_dev_addr"));
                break;
            case WpaEventMessage::WPA_EVENT_SCAN_RESULTS:
                // this->scanResults = this->ScanResults();
                break;
            default:
                break;
            }
        }
        catch (const std::exception &e)
        {
            Log().Error(SS("Failed to process message. " << e.what()));
        }
        this->sychronousMessageToWaitFor = WpaEventMessage::WPA_INVALID_MESSAGE;
    }
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
void P2pSessionManager::OnDeviceFound(P2pDevice &&device)
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

CoTask<> P2pSessionManager::CoOnInit()
{

    InitWpaConfig();

    this->ListNetworks();

    SetUpPersistentGroup();

    P2pListen();
    co_return;
}

P2pDevice::P2pDevice(const WpaEvent &event)
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
        address = event.parameters[0];
        p2p_dev_addr = event.GetNamedParameter("p2p_dev_addr");
        pri_dev_type = event.GetNamedParameter("pri_dev_type");
        name = WpaEvent::UnquoteString(event.GetNamedParameter("name"));
        config_methods = toInt<decltype(config_methods)>(event.GetNamedParameter("config_method"));
        dev_capab = toInt<decltype(dev_capab)>(event.GetNamedParameter("dev_capab"));
        group_capab = toInt<decltype(group_capab)>(event.GetNamedParameter("group_capab"));
        wfd_dev_info = toInt<decltype(wfd_dev_info)>(event.GetNamedParameter("wfd_dev_info"));
        vendor_elems = toInt<decltype(vendor_elems)>(event.GetNamedParameter("vendor_elems"));
        new_ = toInt<decltype(new_)>(event.GetNamedParameter("new"));
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-DEVICE-FOUND message. " << e.what()));
    }
}

void P2pSessionManager::MaybeCreateNetwork(const std::vector<WpaNetwork> &networks)
{
    this->networks = this->ListNetworks();
}
void P2pSessionManager::MaybeEnableNetwork(const std::vector<WpaNetwork> &networks)
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
    } catch (const std::exception& e)
    {
        throw invalid_argument(SS("Invalid P2P-GO-NEG-REQUEST. " << e.what()));
    }
}

void P2pSessionManager::OnP2pGoNegRequest(const P2pGoNegRequest &request)
{

    auto resp = RequestString("GET p2p_go_ht40\n");
    std::string ht40 = (resp == "1") ? " ht40" : "";
    auto command = SS("P2P_CONNECT " << request.src << " '12345678'" << ht40 << '\n');
    try
    {
        Request(command);
    }
    catch (const std::exception &e)
    {
        try
        {
            Request("P2P_CANCEL");
        }
        catch (const std::exception & /*ignored*/)
        {
        }
        throw;
    }
}

void P2pSessionManager::InitWpaConfig()
{
    SetWpaProperty("device_name", WpaEvent::QuoteString("DIRECT-" + gP2pConfiguration.p2p_device_name,'"'));
    SetWpaProperty("country",gP2pConfiguration.country_code);
    SetWpaProperty("device_type",gP2pConfiguration.p2p_device_type);
    SetWpaProperty("config_methods", gP2pConfiguration.p2p_config_methods);
    SetWpaProperty("p2p_go_intent",WpaEvent::ToIntLiteral(gP2pConfiguration.p2p_go_intent));
    SetWpaProperty("persistent_reconnect",gP2pConfiguration.persistent_reconnect? "1": "0");
    SetWpaProperty("p2p_go_ht40", gP2pConfiguration.p2p_go_ht40? "1" : "0" );
    SetWpaProperty("config_methods", gP2pConfiguration.p2p_config_methods);
    SetWpaProperty("update_config", "1");
    UdateWpaConfig();
    SetWpaP2pProperty("ssid_postfix", WpaEvent::QuoteString(gP2pConfiguration.p2p_device_name));
    SetWpaP2pProperty("manufacturer",WpaEvent::QuoteString(gP2pConfiguration.p2p_manufacturer));
    SetWpaP2pProperty("model_name",WpaEvent::QuoteString(gP2pConfiguration.p2p_model_name));
    SetWpaP2pProperty("module_number",gP2pConfiguration.p2p_model_number);
    SetWpaP2pProperty("serial_number",WpaEvent::QuoteString(gP2pConfiguration.p2p_serial_number));
    SetWpaP2pProperty("device_type", gP2pConfiguration.p2p_device_type);
    SetWpaP2pProperty("os_version",gP2pConfiguration.p2p_os_version);
    SetWpaP2pProperty("config_methods",gP2pConfiguration.p2p_config_methods);
    if (gP2pConfiguration.p2p_sec_device_type != "")
    {
        SetWpaP2pProperty("sec_device_type",gP2pConfiguration.p2p_sec_device_type);
    }
    SetWpaP2pProperty("p2p_go_intent",WpaEvent::ToIntLiteral(gP2pConfiguration.p2p_go_intent));
    SetWpaP2pProperty("p2p_ssid_postfix",
            gP2pConfiguration.p2p_ssid_postfix == "" 
            ? gP2pConfiguration.p2p_device_name
            : gP2pConfiguration.p2p_ssid_postfix);
    SetWpaP2pProperty("persistent_reconnect", gP2pConfiguration.persistent_reconnect ? "1" : "0");

    UdateWpaConfig();
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
        Request("P2P_GROUP_ADD\n");
        SynchronousWaitForMessage(WpaEventMessage::P2P_EVENT_GROUP_STARTED, 10000ms);
        this->addingGroup = false;
    }
    catch (const CoTimedOutException & /* ignored*/)
    {
        this->addingGroup = false;
        Log().Warning("Timed out waiting for P2P_EVENT_GROUP_STARTED.");
    }
    catch (...)
    {
        this->addingGroup = false;
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

void P2pSessionManager::OnGroupStarted(const WpaEvent &event)
{
    this->activeGroup = std::make_unique<P2pGroup>(event);

    //<3>P2P-GROUP-STARTED p2p-wlan0-3 GO ssid="DIRECT-NFGroup" freq=5180 passphrase="Z6qktTv6" go_dev_addr=de:a6:32:d4:a1:a4
}
void P2pSessionManager::OnGroupStopped(const WpaEvent &event)
{
    this->activeGroup = nullptr;
}

P2pGroup::P2pGroup(const WpaEvent &event)
{
    //<3>P2P-GROUP-STARTED p2p-wlan0-3 GO ssid="DIRECT-NFGroup" freq=5180 passphrase="Z6qktTv6" go_dev_addr=de:a6:32:d4:a1:a4
    try
    {
        if (event.parameters.size() < 1)
        {
            throw invalid_argument("No interface.");
        }
        if (event.parameters.size() >= 2)
        {
            this->go = event.parameters[1] == "GO";
        }
        ssid = WpaEvent::UnquoteString(event.GetNamedParameter("ssid"), '"');
        freq = toInt<decltype(freq)>(event.GetNamedParameter("freq"));
        passphrase = event.GetNamedParameter("passphrase");
        go_dev_addr = event.GetNamedParameter("go_dev_addr");
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-GROUP-STARTED event. " << e.what()));
    }
}