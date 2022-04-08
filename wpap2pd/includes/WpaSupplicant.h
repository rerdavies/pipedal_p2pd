#pragma once

#include <cotask/CoExec.h>
#include <cotask/CoEvent.h>
#include <cotask/Log.h>
#include "WpaEvent.h"
#include <functional>
#include <cotask/CoBlockingQueue.h>
#include "WpaChannel.h"

// forward declaration.
extern "C" {
    struct wpa_ctrl;
}

namespace p2p {
    using namespace ::cotask;

    /**
     * @brief WpaSupplicant::ScanResult response.
     * 
     */
    class WpaScanInfo {
    public:
        WpaScanInfo() { }
        WpaScanInfo(const std::string &wpaResponse);
        const std::string & Bssid() const { return bssid; }
        int64_t Frequency() const { return frequency; }
        int64_t SignalLevel() const { return signalLevel; }
        const std::vector<std::string>& Flags() const { return flags; }
        const std::string & Ssid() const { return ssid; }

    private:
        std::string bssid;
        int64_t frequency;
        int64_t signalLevel;
        std::vector<std::string> flags;
        std::string ssid;
    };

    /**
     * @brief WpaSupplicant::Status() response.
     * 
     */
    class WpaStatusInfo 
    {
    public:
        WpaStatusInfo() { }
        WpaStatusInfo(const std::vector<std::string> &response);

        const std::string& Bssid() const { return bssid; }
        const std::string& Ssid() const { return ssid; }
        const std::string& PairwiseCipher() const { return pairwiseCipher; }
        const std::string& GroupCipher() const { return groupCipher;}
        const std::string& KeyMgmt() const { return keyMgmt; }
        const std::string& WpaState() const { return wpaState; }
        const std::string& IpAddress() const { return ipAddress; }
        const std::string& SupplicantPaeState() const { return supplicantPaeState; }
        const std::string& SuppPortStatus() const { return suppPortStatus; }
        const std::string& EapState() const { return eapState; }

        const std::vector<std::string>&Extras()const { return extras; }
    private:
        std::string bssid;
        std::string ssid;
        std::string pairwiseCipher;
        std::string groupCipher;
        std::string keyMgmt;
        std::string wpaState;
        std::string ipAddress;
        std::string supplicantPaeState;
        std::string suppPortStatus;
        std::string eapState;
    protected:
        std::vector<std::string> extras;
    };

    /**
     * @brief WpaSupplicant::ListNetworks() response.
     * 
     */
    class WpaNetworkInfo
    {
    public:
        WpaNetworkInfo() { }
        WpaNetworkInfo(const std::string &wpaResponse);
        int Id() const { return number; }
        const std::string&Ssid() const { return ssid; }
        const std::string&Bsid() const { return bsid;}
        const std::vector<std::string>& Flags() const { return flags; }
        
        bool HasFlag(const std::string&flag) const
        {
            return std::find(flags.begin(),flags.end(),flag) != flags.end();
        }
        bool IsDisabled() const { return HasFlag("[DISABLED]");}
        bool IsP2pPersistent() const { return HasFlag("[P2P-PERSISTENT]"); }
        bool IsP2p() const { return HasFlag("[P2P]"); }

    private:
        int number;
        std::string ssid;
        std::string bsid;
        std::vector<std::string> flags;
    };


    class WpaSupplicant : public WpaChannel {
        
    public:
        using base = WpaChannel;
        ~WpaSupplicant();
        CoTask<> Open(const std::string &interfaceName);

        CoTask<> Close();

        /* See: https://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html */
        std::vector<std::string> MIB() { return Request("MIB\n"); }
        WpaStatusInfo Status();
        void Logon() { RequestOK("LOGON\n"); }
        void Logoff() { RequestOK("LOGOFF\n"); }
        void Reassociate() { RequestOK("REASSOCIATE\n"); };
        void PreAuth(std::string bssid);
        void Attach() { RequestOK("ATTACH\n"); }
        void Detach() { RequestOK("DETACH\n"); }
        void Level(std::string debugLevel);
        void Reconfigure() { RequestOK("RECONFIGURE\n");}
        void Terminate() { RequestOK("TERMINATE\n");}
        void Bssid(int network, std::string bssid);
        void Disconnect() { RequestOK("DISCONNECT\n"); }
        void Scan() { RequestOK("SCAN\n"); }
        void EnableNetwork(int networkId);
        void DisableNetwork(int networkId);
        std::vector<WpaScanInfo> ScanResults();
         

        std::vector<WpaNetworkInfo> ListNetworks();

    public:
        std::string GetWpaProperty(const std::string&name);
        void SetWpaProperty(const std::string&name, const std::string &value);
        virtual void UdateWpaConfig();
    public:

        void P2pFind() { RequestOK("P2P_FIND\n"); }
        void P2pFind(int timeoutSeconds);
        void P2pFind(int timeoutSeconds, const std::string & type);
        void P2pListen() { RequestOK("P2P_LISTEN\n"); }
        void P2pStopFind() { RequestOK("P2P_STOP_FIND\n"); }


    protected:
        // hide
        virtual void OpenChannel(const std::string &interface, bool withEvents)
        {
            base::OpenChannel(interface,withEvents);
        }
        virtual void CloseChannel()
        {
            base::CloseChannel();
        }

        virtual CoTask<> CoOnInit() { co_return; }
        virtual CoTask<> CoOnUnInit() { co_return; }
        virtual void OnEvent(const WpaEvent &event) { 
            base::OnEvent(event);
        }
        virtual void PostQuit();

    private:
        bool open = false;
        bool wpaConfigChanged = false;

        

        std::string interfaceName = "wlan0";

        std::vector<WpaNetworkInfo> networks;



        void UpdateStatus();







    };
}// namespace