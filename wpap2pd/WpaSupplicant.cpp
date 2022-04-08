#include "includes/WpaSupplicant.h"
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
        OpenChannel(interfaceName,true);
        open = true;
        co_await CoOnInit();
    }
    catch (const std::exception &e)
    {
        Log().Error(e.what());
    }
    co_return;
}

CoTask<> WpaSupplicant::Close() {
    if (open)
    {
        open = false;
        co_await CoOnUnInit();
        CloseChannel();
    }
}


void WpaSupplicant::PostQuit()
{
    Dispatcher().PostQuit();
}




void WpaSupplicant::UpdateStatus()
{
    networks = ListNetworks();
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
std::vector<WpaNetworkInfo> WpaSupplicant::ListNetworks()
{
    auto response = Request("LIST_NETWORKS\n");

    vector<WpaNetworkInfo> result;
    for (size_t i = 1; i < response.size(); ++i)
    {
        result.push_back(WpaNetworkInfo(response[i]));
    }
    return result;
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
WpaStatusInfo WpaSupplicant::Status()
{
    auto response = Request("STATUS");
    return WpaStatusInfo(response);
}

void WpaSupplicant::Bssid(int network, std::string bssid)
{
    std::stringstream s;
    s << "BSSID " << network << " " << bssid;

    RequestOK(s.str());
}

std::vector<WpaScanInfo> WpaSupplicant::ScanResults()
{
    auto response = Request("SCAN_RESULTS\n");
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
    return result;
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


void WpaSupplicant::PreAuth(std::string bssid)
{
    RequestOK(SS("PREAUTH " << bssid << '\n'));
}

void WpaSupplicant::Level(std::string debugLevel)
{
    RequestOK(SS("LEVEL " << debugLevel << '\n'));
}





void WpaSupplicant::P2pFind(int timeoutSeconds)
{
    RequestOK(SS("P2P_FIND " << timeoutSeconds << "\n"));
}
void WpaSupplicant::P2pFind(int timeoutSeconds, const std::string &type)
{
    RequestOK(SS("P2P_FIND " << timeoutSeconds << " " << type << "\n"));

}

void WpaSupplicant::EnableNetwork(int networkId)
{
    try {
        RequestOK(SS("ENABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
        auto message = SS("Failed to eable network. " << e.what() );
        throw CoIoException(e.errNo(),message);
    }
}
void WpaSupplicant::DisableNetwork(int networkId)
{
    try {
        RequestOK(SS("DISABLE_NETWORK " << networkId << "\n"));
    } catch (const CoIoException &e)
    {
    }
}


std::string WpaSupplicant::GetWpaProperty(const std::string &name)
{
    try {
    auto result = Request(SS("GET " << name << '\n'));
    if (result.size() == 1)
    {
        return result[0];
    }
    } catch (const std::exception &e)
    {
        Log().Debug(SS("Can't get property " << name));
    }
    return "";
}

void WpaSupplicant::SetWpaProperty(const std::string&name, const std::string &value)
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
void WpaSupplicant::UdateWpaConfig()
{
    if (wpaConfigChanged)
    {
        wpaConfigChanged = false;
        if (gP2pConfiguration.updateConfig)
        {
            RequestOK("SET update_config 1\n");
        }
    }

}


