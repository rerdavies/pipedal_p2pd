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
        CoTask<std::vector<std::string>> MIB() { return Request("MIB\n"); }
        CoTask<WpaStatusInfo> Status();
        CoTask<> Logon() { return RequestOK("LOGON\n"); }
        CoTask<> Logoff() { return RequestOK("LOGOFF\n"); }
        CoTask<> Reassociate() { return RequestOK("REASSOCIATE\n"); };
        CoTask<> PreAuth(std::string bssid);
        CoTask<> Attach() { return RequestOK("ATTACH\n"); }
        CoTask<> Detach() { return RequestOK("DETACH\n"); }
        CoTask<> Level(std::string debugLevel);
        CoTask<> Reconfigure() { return RequestOK("RECONFIGURE\n");}
        CoTask<> Terminate() { return RequestOK("TERMINATE\n");}
        CoTask<> Bssid(int network, std::string bssid);
        CoTask<> Disconnect() { return RequestOK("DISCONNECT\n"); }
        CoTask<> Scan() { return RequestOK("SCAN\n"); }
        CoTask<> EnableNetwork(int networkId);
        CoTask<> DisableNetwork(int networkId);
        CoTask<std::vector<WpaScanInfo>> ScanResults();
         

        CoTask<std::vector<WpaNetworkInfo>> ListNetworks();

    public:
        CoTask<std::string> GetWpaProperty(const std::string&name);
        CoTask<> SetWpaProperty(const std::string&name, const std::string &value);
        virtual CoTask<> UdateWpaConfig();
    public:

        CoTask<> P2pFind() { return RequestOK("P2P_FIND\n"); }
        CoTask<> P2pFind(int timeoutSeconds);
        CoTask<> P2pFind(int timeoutSeconds, const std::string & type);
        CoTask<> P2pListen() { return RequestOK("P2P_LISTEN\n"); }
        CoTask<> P2pStopFind() { return RequestOK("P2P_STOP_FIND\n"); }

        bool IsFinished() { return isFinished; }
    protected:

        void SetFinished() {
            isFinished = true;
        }


        CoTask<> KeepAliveProc();

        // hide
        virtual CoTask<> OpenChannel(const std::string &interface, bool withEvents)
        {
            return base::OpenChannel(interface,withEvents);
        }
        virtual void CloseChannel()
        {
            base::CloseChannel();
        }

        virtual CoTask<> CoOnInit() { co_return; }
        virtual CoTask<> CoOnUnInit() { co_return; }
        virtual CoTask<> OnEvent(const WpaEvent &event) { 
            co_await base::OnEvent(event);
        }
        virtual void PostQuit();

    private:
        bool isFinished = false;

        bool open = false;
        bool wpaConfigChanged = false;

        

        std::string interfaceName = "wlan0";

        std::vector<WpaNetworkInfo> networks;









    };
}// namespace