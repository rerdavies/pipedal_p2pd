#pragma once 

#include "WpaEvent.h"
#include "WpaExceptions.h"
#include "cotask/Log.h"
#include <memory>
#include "cotask/CoTask.h"
#include "cotask/CoFile.h"
#include "cotask/CoBlockingQueue.h"

struct wpa_ctrl;

namespace p2p
{

    class WpaChannel {
    public:
        WpaChannel();
        virtual ~WpaChannel();
        WpaChannel(wpa_ctrl*ctrl);


        virtual void OpenChannel(const std::string &interfaceName, bool withEvents = true);
        void CloseChannel();

        virtual void SetLog(std::shared_ptr<ILog> log) { this->pLog = log; rawLog = log.get(); /* (ILog is thread-safe) */ }
        ILog&Log() const { return *rawLog; }

        /**
         * @brief Has the connection to the wpa_supplicant service been lost? 
         * 
         * @return true if connection lost.
         * @return false if connected.
         */
        bool IsDisconnected();

        /**
         * @brief Delay, and abort if disconnected.
         * 
         * Use instead of CoDelay for Wap operations. Guaranteed not to outlive
         * the lifetime of the WpaSupplicant.
         * 
         * @param time how long to wait.
         * @throws WpaDisconnectedException if the current connect has been disconnected.
         */
        CoTask<> Delay(std::chrono::milliseconds time);

        CoTask<> Delay(std::chrono::seconds time) {
            return Delay(std::chrono::duration_cast<std::chrono::milliseconds>(time));
        }

        /**
         * @brief Ping the channel to make sure it's alive.
         * 
         * @throws CoTimedOutException if the ping times out.
         * @throws WpaIoException if the channel is dead.
         */
        void Ping();


        /**
         * @brief Send a request to wpa_supplicant.
         * 
         * @param request The request to execute.
         * @return std::vector<std::string> Data returned by wpa_supplicant
         * @throws WpaDisconnectedException if the wap_suplicant connection drops.
         * @throws WpaIoException if an i/o error occurs on the wpa_supplicant channel.
         */
        std::vector<std::string> Request(const std::string &request);

        /**
         * @brief Send a request to wpa_supplicant, checking for an OK response.
         * 
         * An exception is thrown if the response is not a single value, or if the 
         * response is not "OK".
         * 
         * @param request The request to execute.
         * @return std::vector<std::string> Data returned by wpa_supplicant
         * @throws WpaDisconnectedException if the wpp_suplicant connection drops.
         * @throws WpaIoException if an i/oerror occurs on the wpa_supplicant channel.
         * @throws WpaFailedException if the channel does not return an OK response.
         */
        void  RequestOK(const std::string &request);

        /**
         * @brief Send a request to wpa_supplication returning a single string respose.
         * 
         * Returns a single string response. An exception is thrown if the response is not a single value.
         * 
         * If the throwIfFaile parameter is true, a WpaFile exception is thrown if the response is "FAIL", or "INVALID_COMMAND" (a common, but not 
         * universal  failure pattern for wpa_supplicant requests).)
         * 
         * @throws WpaIoExcpetion if there's an i/o error on the channel, or if the response is not a single string.
         * @throws WpaFailedException if throwIfFaield is true, and the reponse is "FAIL" or "INVALID COMMAND".
         * @param request The response from wpa_supplication.
         * @return std::string 
         */

        std::string RequestString(const std::string&request, bool throwIfFailed = false);

        bool TraceMessages() const { return traceMessages; }
        void TraceMessages(bool value, const std::string&logPrefix) { traceMessages = value; this->logPrefix = logPrefix; }
        void TraceMessages(bool value) {TraceMessages(value,""); }
    protected:
        void OpenChannel(wpa_ctrl*ctrl);
        void CloseChannel(wpa_ctrl*ctrl);

        virtual void OnEvent(const WpaEvent&event) { }
        std::shared_ptr<ILog> GetSharedLog() { return pLog; }
    private:
        wpa_ctrl*ctrl = nullptr;
        bool closed = true;
        bool withEvents = false;
        std::string interfaceName;

        bool traceMessages = false;
        std::string logPrefix;

        std::shared_ptr<ILog> pLog;
        ILog*rawLog = nullptr; // ILog is thread-safe shared_ptr<ILog> is not!

        bool disconnected = false;
        CoConditionVariable cvDelay;

        CoFile socketFile;
        wpa_ctrl*commandSocket = nullptr;

        CoBlockingQueue<WpaEvent> eventMessageQueue { 512};

        bool lineBufferRecoveringFromTimeout = false;
        int lineBufferHead = 0;
        int lineBufferTail = 0;
        char lineBuffer[512];
        bool lineResultEmpty = true;
        std::stringstream lineResult;

        bool recvAbort = false;
        bool recvReady = true;
        int  recvThreadCount = 0;
        CoConditionVariable cvRecv;

        CoConditionVariable cvRecvRunning;

        void AbortRecv()
        {
            cvRecv.Notify(
                [this] () {
                    recvAbort = true;
                }
            );
        }
        CoTask<> JoinRecvThread();

        CoTask<std::vector<std::string>> ReadToPrompt(std::chrono::milliseconds timeout);
        CoTask<> ReadEventsProc(wpa_ctrl*ctrl,std::string interfaceName);
        CoTask<> ForegroundEventHandler();

        wpa_ctrl *OpenWpaSocket(const std::string &interfaceName);

        void SetDisconnected();


    };


} // namespace


