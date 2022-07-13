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

#include "includes/WpaSupplicant.h"
#include "ss.h"
#include "cotask/Os.h"
#include "cotask/CoEvent.h"
#include "cotask/CoExceptions.h"
#include "ss.h"
#include "includes/P2pUtil.h"
#include "includes/P2pConfiguration.h"


using namespace cotask;
using namespace p2p;
using namespace std;




WpaSupplicant::~WpaSupplicant()
{
}

CoTask<> WpaSupplicant::Open(const std::string&interfaceName)
{
    try
    {
        this->interfaceName = interfaceName;

        for (int retries = 0; /**/; ++retries)
        {
            try {
                co_await OpenChannel(interfaceName,true);
                break;
            }
            catch (const std::exception &e)
            {
                if (retries == 0)
                {
                    Log().Info(SS("Failed to connect to interface " + interfaceName + ". Retrying ... (" << (retries+1) << " of 10)"));
                } else 
                {
                    if (retries == 10)
                    {
                        throw;
                    }
                }
            }
            co_await CoDelay(5000ms);
        }
        Log().Info("Connected to " +interfaceName);

            co_await CoDelay(3000ms);

        open = true;

        CoDispatcher::CurrentDispatcher().StartThread(KeepAliveProc());

        co_await CoOnInit();
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("Open failed. (" << e.what() << ")"));
        throw;
    }
    co_return;
}

CoTask<> WpaSupplicant::Close() {
    if (open)
    {
        open = false;
        Log().Error(SS("Uninitializing..."));
        co_await CoOnUnInit();
        Log().Error(SS("Closing wpa_supplicant channel..."));
        CloseChannel();
    }
}


void WpaSupplicant::PostQuit()
{
    Dispatcher().PostQuit();
}




WpaNetworkInfo::WpaNetworkInfo(const std::string &wpaResponse)
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
CoTask<std::vector<WpaNetworkInfo>> WpaSupplicant::ListNetworks()
{
    auto response = co_await Request("LIST_NETWORKS\n");

    vector<WpaNetworkInfo> result;
    for (size_t i = 1; i < response.size(); ++i)
    {
        result.push_back(WpaNetworkInfo(response[i]));
    }
    co_return result;
}

WpaStatusInfo::WpaStatusInfo(const std::vector<std::string> &wpaResponse)
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
CoTask<WpaStatusInfo> WpaSupplicant::Status()
{
    auto response = co_await Request("STATUS");
    co_return WpaStatusInfo(response);
}

CoTask<> WpaSupplicant::Bssid(int network, std::string bssid)
{
    std::stringstream s;
    s << "BSSID " << network << " " << bssid;

    co_await RequestOK(s.str());
}

CoTask<std::vector<WpaScanInfo>> WpaSupplicant::ScanResults()
{
    auto response = co_await Request("SCAN_RESULTS\n");
    std::vector<WpaScanInfo> result;
    for (size_t i = 1; i < response.size(); ++i)
    {
        try
        {
            result.push_back(WpaScanInfo(response[i]));
        }
        catch (std::exception &e)
        {
            // truncated result (due to UDP packet size limit?)
        }
    }
    co_return result;
}


WpaScanInfo::WpaScanInfo(const std::string &wpaResponse)
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


CoTask<> WpaSupplicant::PreAuth(std::string bssid)
{
    co_await RequestOK(SS("PREAUTH " << bssid << '\n'));
}

CoTask<> WpaSupplicant::Level(std::string debugLevel)
{
    co_await RequestOK(SS("LEVEL " << debugLevel << '\n'));
}





CoTask<>  WpaSupplicant::P2pFind(int timeoutSeconds)
{
    co_await RequestOK(SS("P2P_FIND " << timeoutSeconds << "\n"));
}
CoTask<> WpaSupplicant::P2pFind(int timeoutSeconds, const std::string &type)
{
    co_await RequestOK(SS("P2P_FIND " << timeoutSeconds << " " << type << "\n"));

}

CoTask<>  WpaSupplicant::EnableNetwork(int networkId)
{
    try {
        co_await RequestOK(SS("ENABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
        auto message = SS("Failed to eable network. " << e.what() );
        throw CoIoException(e.errNo(),message);
    }
}
CoTask<> WpaSupplicant::DisableNetwork(int networkId)
{
    try {
        co_await RequestOK(SS("DISABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
    }
}


CoTask<std::string> WpaSupplicant::GetWpaProperty(const std::string &name)
{
    try {
    auto result = co_await Request(SS("GET " << name << '\n'));
    if (result.size() == 1)
    {
        co_return result[0];
    }
    } catch (const std::exception &e)
    {
        Log().Debug(SS("Can't get property " << name));
    }
    co_return "";
}

CoTask<> WpaSupplicant::SetWpaProperty(const std::string&name, const std::string &value)
{
    if (co_await GetWpaProperty(name) != value)
    {
        try {
            co_await RequestOK(SS("SET " << name << " " << value << "\n"));
            wpaConfigChanged = true;
        } catch (const std::exception & e)
        {
            Log().Error(SS("Cant set wpa property " << name << ". " << e.what()));
        }

    }
}
CoTask<> WpaSupplicant::UdateWpaConfig()
{
    // nothing to be done. Config is updated iff a connection is made.
    co_return;

}


CoTask<> WpaSupplicant::KeepAliveProc()
{
    // xxx: meh.
    // Ping every 15 seconds, just to make sure wpa_supplicant is responsive.
    try
    {
        while (true)
        {
            co_await this->Delay(17s);
            co_await Ping(); // check for dead sockets.
        }
    }
    catch (const WpaDisconnectedException &)
    {
    }
    catch (const std::exception &e)
    {
        Log().Error(SS("wpa_supplicant is not responding. " << e.what()));
    }
    this->SetFinished();
    co_return;
}

