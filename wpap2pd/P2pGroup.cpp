#include "includes/P2pGroup.h"
#include "includes/P2pSessionManager.h"
#include "includes/P2pConfiguration.h"
#include "ss.h"
#include "includes/P2pUtil.h"

using namespace p2p;
using namespace std;

void P2pGroup::SetParameters()
{
    // SetWpaP2pProperty("ssid_postfix", WpaEvent::QuoteString(gP2pConfiguration.p2p_device_name));
    // SetWpaP2pProperty("config_methods",gP2pConfiguration.p2p_config_methods);
    // SetWpaP2pProperty("p2p_go_intent",WpaEvent::ToIntLiteral(gP2pConfiguration.p2p_go_intent));
    // SetWpaP2pProperty("p2p_ssid_postfix",
    //         gP2pConfiguration.p2p_ssid_postfix == "" 
    //         ? gP2pConfiguration.p2p_device_name
    //         : gP2pConfiguration.p2p_ssid_postfix);
    // SetWpaP2pProperty("persistent_reconnect", gP2pConfiguration.persistent_reconnect ? "1" : "0");


}

void P2pGroup::SetP2pProperty(const std::string &name, const std::string &value)
{
    auto response = Request(SS("P2P_SET " << name << " " << value << "\n"));
    if (response.size() != 1 || response[0] != "OK")
    {
        Log().Warning(SS("Can't set p2p property " << name));
    }
}

P2pGroupInfo::P2pGroupInfo(const WpaEvent &event)
{
    //<3>P2P-GROUP-STARTED p2p-wlan0-3 GO ssid="DIRECT-NFGroup" freq=5180 passphrase="Z6qktTv6" go_dev_addr=de:a6:32:d4:a1:a4
    try
    {
        if (event.parameters.size() < 1)
        {
            throw invalid_argument("No interface.");
        }
        this->interface = event.parameters[0];
        if (event.parameters.size() >= 2)
        {
            this->go = event.parameters[1] == "GO";
        }
        ssid = WpaEvent::UnquoteString(event.GetNamedParameter("ssid"));
        freq = toInt<decltype(freq)>(event.GetNamedParameter("freq"));
        passphrase = event.GetNamedParameter("passphrase");
        go_dev_addr = event.GetNamedParameter("go_dev_addr");
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-GROUP-STARTED event. " << e.what()));
    }
}
void P2pGroup::OpenChannel() 
{
    base::OpenChannel(groupInfo.interface,true);

    PreAuth();
}
void P2pGroup::PreAuth() 
{
    auto & config = gP2pConfiguration;
    if (config.p2p_config_method == "keypad")
    {
        try {
            Request(SS("WPS_PIN any " << config.p2p_pin << "\n"));
        } catch (std::exception &e /*ignored*/) {}
    } else if (config.p2p_config_method == "none")
    {
        try {
            // pre-auth a PBC request.
            Request(SS("WPS_PBC\n"));
        } catch (std::exception &e /*ignored*/) {}
    }
}

void P2pGroup::OnEvent(const WpaEvent &event)
{
    try {
        auto &config = gP2pConfiguration;
        base::OnEvent(event);
        switch (event.message)
        {
        //https://raspberrypi.stackexchange.com/questions/117119/how-to-invoke-the-existing-persistent-p2p-go-on-restart-of-device-to-create-aut
        case WpaEventMessage::WPS_EVENT_ENROLLEE_SEEN: 
            try {
                if (config.p2p_config_method == "keypad")
                {
                    Request(SS("WPS_PIN any " << gP2pConfiguration.p2p_pin << '\n'));
                } else if (config.p2p_config_method == "none")
                {
                    Request(SS("WPS_PBC\n"));
                }
            } catch (const std::exception&e)
            {
                Log().Error(e.what());
            }
            break;
        default:
            break;
        }
    } catch (const std::exception &e)
    {
        
    }
}
