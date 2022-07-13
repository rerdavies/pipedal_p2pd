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

#include "includes/P2pGroup.h"
#include "includes/P2pSessionManager.h"
#include "includes/P2pConfiguration.h"
#include "ss.h"
#include "includes/P2pUtil.h"
#include <vector>
#include <string>

using namespace p2p;
using namespace std;

namespace p2p
{
    class PinNeededRequest
    {
    public:
        PinNeededRequest(const WpaEvent&event)
        {
            // <3>WPS-PIN-NEEDED 37bd02b7-b38f-5b51-8947-bab17e71b8e1 b2:19:a1:91:e0:0a [ | | | | |0-00000000-0]
            uuid = event.getParameter(0);
            deviceId = event.getParameter(1);
            options = event.options;

        }

        std::string uuid;
        std::string deviceId;
        std::vector<std::string> options;
    };
}

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

CoTask<> P2pGroup::SetP2pProperty(const std::string &name, const std::string &value)
{
    auto response = co_await Request(SS("P2P_SET " << name << " " << value << "\n"));
    if (response.size() != 1 || response[0] != "OK")
    {
        Log().Warning(SS("Can't set p2p property " << name));
    }
    co_return;
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
        freq = ToInt<decltype(freq)>(event.GetNamedParameter("freq"));
        passphrase = event.GetNamedParameter("passphrase");
        go_dev_addr = event.GetNamedParameter("go_dev_addr");
    }
    catch (const std::exception &e)
    {
        throw invalid_argument(SS("Invalid P2P-GROUP-STARTED event. " << e.what()));
    }
}

void P2pGroup::DebugHook()
{
    //Log().Debug("Ping.");
}

CoTask<> P2pGroup::PingProc()
{
    try
    {
        while (true)
        {
            while (true)
            {
                co_await this->Delay(23s);
                co_await Ping();
            }
        }
    }
    catch (const std::exception &e)
    {
        Log().Debug(SS("Group ping proc terminated. " << e.what()));
    }
}
CoTask<> P2pGroup::OpenChannel()
{
    co_await base::OpenChannel(interfaceName, true);

    Dispatcher().StartThread(PingProc());

    std::string configMethod = gP2pConfiguration.p2p_config_method;
    if (configMethod == "none")
    {
        configMethod = "pbc";
    } else if (configMethod == "label")
    {
        configMethod = "keypad";
    }

    co_await RequestOK(SS("SET config_methods " << configMethod << "\n" ));

    std::vector<StationInfo> stations = co_await ListSta();
    //co_await PreAuth();
    co_return;
}
CoTask<> P2pGroup::PreAuth(const std::string &clientId)
{
    auto &config = gP2pConfiguration;

    try
    {
        DebugHook();

        // bool active = false;
        // bool pbc = false;
        // std::string pin;

        // {
        //     std::lock_guard lock{pSessionManager->mxEnrollmentRecord};
        //     auto &er = pSessionManager->enrollmentRecord;

        //     if (er.active)
        //     {
        //         // copy data out.
        //         active = true;
        //         pbc = er.pbc;
        //         pin = er.pin;

        //         // set the er back to idle state.
        //         er.active = false;
        //         er.pin = "";
        //         er.deviceId = "";
        //     }
        // }

        if (true)
        {
            currentEnrollee = clientId;
            if (gP2pConfiguration.p2p_config_method == "label"
            || gP2pConfiguration.p2p_config_method == "keypad")
            {
                co_await Request(SS("WPS_PIN "
                                    << clientId
                                    // << "any"
                                    << " " << gP2pConfiguration.p2p_pin
                                     << '\n'));
            }
            else if (config.p2p_config_method == "none")
            {
                co_await Request(SS("WPS_PBC " << clientId << "\n"));
            }
        }
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Preauth failed: " << e.what()));
    }
    co_return;
}

CoTask<> P2pGroup::OnEnrolleeSeen(const WpaEvent &event)
{
    //  <3>WPS-ENROLLEE-SEEN 06:13:8e:56:bc:4d 777da1be-064d-56bd-9912-d78d65a57d45 0-00000000-0 0x3148 0 0 [
    if (event.parameters.size() < 1)
        co_return;

    std::string bssId = event.parameters[0];
    co_await PreAuth(event.getParameter(0));
}
CoTask<> P2pGroup::OnEvent(const WpaEvent &event)
{
    try
    {
        co_await base::OnEvent(event);
        switch (event.message)
        {
        //https://raspberrypi.stackexchange.com/questions/117119/how-to-invoke-the-existing-persistent-p2p-go-on-restart-of-device-to-create-aut
        case WpaEventMessage::WPS_EVENT_ENROLLEE_SEEN:
            co_await OnEnrolleeSeen(event);
            break;
        case WpaEventMessage::WPA_EVENT_DISCONNECTED:
            Log().Debug(SS("Group disconnected. " << event.ToString()));
            co_await TerminateGroup();
            break;
        case WpaEventMessage::FAIL:
            if (event.messageString == "FAIL-CHANNEL-UNSUPPORTED")
            {
                Log().Error("Channel unsupported. (possibly already in use)");
                co_await TerminateGroup();
            }
            else if (event.messageString == "FAIL-CHANNEL-UNAVAILABLE")
            {
                Log().Error("Channel unavailable. (regDomain, or already in use?)");
                co_await TerminateGroup();
            }
            else
            {
                Log().Error(SS("Unexpected error: " << event.messageString));
            }
            break;
        case WpaEventMessage::WPS_EVENT_PIN_NEEDED:
        {
            PinNeededRequest pinNeeded(event);
            Log().Debug("Pin request received.");
            co_await Request(SS("WPS_PIN " << pinNeeded.deviceId << " " << gP2pConfiguration.p2p_pin << "\n"));
            break;
        }
        break;

        case WpaEventMessage::AP_EVENT_DISABLED:
            Log().Error("Access point disabled.");
            co_await TerminateGroup();
            break;

        // messages we expect.
        case WpaEventMessage::WPA_EVENT_EAP_PROPOSED_METHOD:
        case WpaEventMessage::WPA_EVENT_SCAN_STARTED:
        case WpaEventMessage::WPA_EVENT_EAP_RETRANSMIT:
        case WpaEventMessage::WPA_EVENT_EAP_RETRANSMIT2:
        case WpaEventMessage::WPA_EVENT_SCAN_RESULTS:
        case WpaEventMessage::RX_PROBE_REQUEST:
        case WpaEventMessage::WPA_EVENT_SUBNET_STATUS_UPDATE:
        case WpaEventMessage::WPA_EVENT_EAP_STARTED:
        case WpaEventMessage::WPS_EVENT_REG_SUCCESS:
        case WpaEventMessage::WPS_EVENT_SUCCESS:
            break;
        case WpaEventMessage::WPA_EVENT_EAP_FAILURE2:
        case WpaEventMessage::WPA_EVENT_EAP_FAILURE:
            Log().Info(SS("EAP failure. " << event.ToString()));
            break;
        case WpaEventMessage::AP_STA_CONNECTED:
            // Better handled in the Group message handlers, where we know the device name.
            //Log().Info(SS("Station connected. " << event.getParameter(0)));
            this->currentEnrollee = "";
            break;
        case WpaEventMessage::AP_STA_DISCONNECTED:
            //Log().Info(SS("Station disconnected. " << event.getParameter(0)));
            this->currentEnrollee = "";
            break;
        case WpaEventMessage::WPA_EVENT_BSS_ADDED:
            // not interesting.
            break;
        default:
            if (TraceMessages())
            {
                Log().Debug(SS("Unhandled event: " << event.ToString()));
            }
            break;
        }
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Exception during group event processing: " << e.what()));
    }
    co_return;
}

CoTask<> P2pGroup::TerminateGroup()
{
    if (pSessionManager != nullptr)
    {
        co_await pSessionManager->CloseGroup(this);
    }
    co_return;
}
