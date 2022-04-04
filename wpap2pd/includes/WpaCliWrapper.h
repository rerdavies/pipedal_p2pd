#pragma once

#include <cotask/CoExec.h>
#include <cotask/CoEvent.h>
#include <cotask/Log.h>
#include "WpaEvent.h"
#include <functional>
#include <cotask/CoBlockingQueue.h>

// forward declaration.
extern "C" {
    struct wpa_ctrl;
}

namespace p2p {
    using namespace ::cotask;

    /**
     * @brief Exception indicating in invalid wap_supplication action.
     * 
     */
    class WpaIoException : public CoIoException {
    public:
        using base = CoIoException;
        
        WpaIoException(int errNo,const std::string what)
        : base(errNo,what) {}
    };
    /**
     * @brief Operation has been aborted due to disconnection.
     * 
     */
    class WpaDisconnectedException : public CoException {
    public:
        using base = CoException;
        
        WpaDisconnectedException() { }
        virtual const char*what() const noexcept {
            return "Disconnected.";
        }
    };
    /**
     * @brief Operation has timed out.
     * 
     */
    class WpaTimedOutException : public CoTimedOutException {
    public:
    };

    /**
     * @brief WpaCliWrapper::ScanResult response.
     * 
     */
    class WpaScanResult {
    public:
        WpaScanResult() { }
        WpaScanResult(const std::string &wpaResponse);
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
     * @brief WpaCliWrapper::Status() response.
     * 
     */
    class WpaStatus
    {
    public:
        WpaStatus() { }
        WpaStatus(const std::vector<std::string> &response);

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
     * @brief WpaCliWrapper::ListNetworks() response.
     * 
     */
    class WpaNetwork
    {
    public:
        WpaNetwork() { }
        WpaNetwork(const std::string &wpaResponse);
        int Number() const { return number; }
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


    class WpaCliWrapper  {
        using base = CoExec;
    public:
        ~WpaCliWrapper();
        CoTask<> Open(int argc, const char*const*argv);

        void SetLog(std::shared_ptr<ILog> log) { this->pLog = log; }
        ILog&Log() const { return *(pLog.get()); }

        // Wpa_ctl requests..
        /**
         * @brief Pings wpa_supplicant to see if it's alive.
         * 
         * @throws CoTimedOut if no response.
         * 
         */
        /* See: https://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html */
        void Ping();
        std::vector<std::string> MIB() { return Request("MIB\n"); }
        WpaStatus Status();
        void Logon() { RequestOK("LOGON\n"); }
        void Logoff() { RequestOK("LOGOFF\n"); }
        void Reassociate() { RequestOK("REASSOCIATE\n"); };
        void PreAuth(std::string bssid);
        void Attach() { RequestOK("ATTACH\n"); }
        void Detach() { RequestOK("ATTACH\n"); }
        void Level(std::string debugLevel);
        void Reconfigure() { RequestOK("RECONFIGURE\n");}
        void Terminate() { RequestOK("TERMINATE\n");}
        void Bssid(int network, std::string bssid);
        void Disconnect() { RequestOK("DISCONNECT\n"); }
        void Scan() { RequestOK("SCAN\n"); }
        void EnableNetwork(int networkId);
        void DisableNetwork(int networkId);
        std::vector<WpaScanResult> ScanResults();
         

        std::vector<WpaNetwork> ListNetworks();

        /**
         * @brief Send a request to wpa_supplicant.
         * 
         * @param request The request to execute.
         * @return std::vector<std::string> Data returned by wpa_supplicant
         */
        std::vector<std::string> Request(const std::string &request);
        /**
         * @brief Send a request to wpa_supplicant and check for on OK response.
         * 
         * Many (but not all) wpa_supplicant request return an OK/FAIL response.
         * This RequestOK() sends the request, and then throws an exception
         * if the response is not OK.
         *
         * @param request The request to send.
         * @throws WpaIoException if wpa_supplicant doesn't send an OK response.
         * @throws WpaDisconnectedException if the wap_suplicant connection drops.
         */
        void  RequestOK(const std::string &request);
        std::string RequestString(const std::string &request);
    public:
        std::string GetWpaProperty(const std::string&name);
        void SetWpaProperty(const std::string&name, const std::string &value);
        void SetWpaP2pProperty(const std::string &name, const std::string &value);
        virtual void UdateWpaConfig();
    public:

        void P2pFind() { RequestOK("P2P_FIND\n"); }
        void P2pFind(int timeoutSeconds);
        void P2pFind(int timeoutSeconds, const std::string & type);
        void P2pListen() { RequestOK("P2P_LISTEN\n"); }
        void P2pStopFind() { RequestOK("P2P_STOP_FIND\n"); }


        /**
         * @brief Delay, and abort if disconnected.
         * 
         * Use instead of CoDelay for Wap operations. Guaranteed not to outlive
         * the lifetime of the WpaCliWrapper.
         * 
         * @param time how long to wait.
         * @throws WpaDisconnectedException if the current connect has been disconnected.
         */
        CoTask<> Delay(std::chrono::milliseconds time);

        /**
         * @brief Has the connection to the wpa_supplicant service been lost? 
         * 
         * @return true if connection lost.
         * @return false if connected.
         */
        bool IsDisconnected();
    protected:
        enum class Source {
            P2p,
            Wlan
        };
        virtual CoTask<> CoOnInit() { co_return; }
        virtual void OnEvent(Source source,WpaEvent &event) { return;}
        virtual void PostQuit();

        virtual void OnDisconnected();

    private:
        bool wpaConfigChanged = false;

        CoTask<> KeepAliveProc();
        bool disconnected = false;
        CoConditionVariable cvDelay;

        std::vector<std::string> Request(wpa_ctrl*ctl,const std::string &message);

        std::string p2pInterfaceName = "p2p-dev-wlan0";
        std::string wlanInterfaceName = "wlan0";

        std::vector<WpaNetwork> networks;

        CoFile p2pSocket;
        CoFile wlanSocket;

        std::shared_ptr<ILog> pLog = std::make_shared<ConsoleLog>();

        bool traceMessages = true;

        CoTask<std::vector<std::string>> ReadToPrompt(std::chrono::milliseconds timeout);
        CoTask<> ReadP2pEventsProc(wpa_ctrl*ctrl,std::string interfaceName);
        CoTask<> ForegroundEventHandler();

        wpa_ctrl*p2pCommandSocket = nullptr;
        wpa_ctrl* OpenWpaCtrl(const std::string &interfaceName);
        void CloseWpaCtrl(wpa_ctrl*ctrl);
        void UpdateStatus();


        CoBlockingQueue<WpaEvent> p2pControlMessageQueue { 512};


        bool lineBufferRecoveringFromTimeout = false;
        int lineBufferHead = 0;
        int lineBufferTail = 0;
        char lineBuffer[512];
        bool lineResultEmpty = true;
        std::stringstream lineResult;

        bool recvReady = true;
        bool recvAbort = false;
        CoConditionVariable cvRecv;

        bool recvRunning = false;
        CoConditionVariable cvRecvRunning;

        void AbortRecv()
        {
            cvRecv.Notify(
                [this] () {
                    recvAbort = true;
                    recvReady = true;
                }
            );
        }
        CoTask<> JoinRecvThread();


    };
}// namespace