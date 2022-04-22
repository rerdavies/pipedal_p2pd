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

#include "WpaEvent.h"
#include "WpaExceptions.h"
#include "cotask/Log.h"
#include <memory>
#include "cotask/CoTask.h"
#include "cotask/CoFile.h"
#include "cotask/CoBlockingQueue.h"
#include "includes/WpaCtrl.h"


namespace p2p
{

    class StationInfo {
    public:
        StationInfo(const char*buffer);

        std::string address;
        std::string p2p_device_name;
        size_t rx_bytes = 0;
        size_t tx_bytes = 0;
        size_t rx_packets = 0;
        size_t tx_packets = 0;

        std::vector<std::string> parameters;
        std::vector<std::pair<std::string,std::string>> namedParameters;

        std::string GetParameter(size_t index) const;
        std::string GetNamedParameter(const std::string&key) const;
        std::string ToString() const;
    private:
        size_t GetSizeT(const std::string &key) const;
        void Parse(const char*buffer);
        void InitVariables();
    };


    class WpaChannel {
    public:
        WpaChannel();
        virtual ~WpaChannel();

        virtual CoTask<> OpenChannel(const std::string &interfaceName, bool withEvents = true);
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
        CoTask<> Ping();


        /**
         * @brief Send a request to wpa_supplicant.
         * 
         * @param request The request to execute.
         * @return std::vector<std::string> Data returned by wpa_supplicant
         * @throws WpaDisconnectedException if the wap_suplicant connection drops.
         * @throws WpaIoException if an i/o error occurs on the wpa_supplicant channel.
         */
        CoTask<std::vector<std::string>> Request(const std::string request);

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
        CoTask<>  RequestOK(const std::string request);

        /**
         * @brief Send a request to wpa_supplication returning a single string respose.
         * 
         * Returns a single string response. An exception is thrown if the response is not a single value.
         * 
         * If the throwIfFaile parameter is true, a WpaFile exception is thrown if the response is "FAIL", or "INVALID_COMMAND" (a common, but not 
         * universal  failure pattern for wpa_supplicant requests).)
         * 
         * @throws WpaIoExcpetion if there's an i/o error on the channel, or if the response is not a single string.
         * @throws WpaFailedException if throwIfFailed is true, and the reponse is "FAIL" or "INVALID COMMAND".
         * @param request The response from wpa_supplication.
         * @return std::string 
         */

        CoTask<std::string> RequestString(const std::string request, bool throwIfFailed = false);

        /**
         * @brief Get a list of Connected stations.
         * 
         * The equivalent of wpa_cli's list_sta command, which 
         * requires multiple requests against a wpa_supplicant socket.
         */
        CoTask<std::vector<StationInfo>> ListSta();


        bool TraceMessages() const { return traceMessages; }
        void TraceMessages(bool value, const std::string&logPrefix) { traceMessages = value; this->logPrefix = logPrefix; }
        void TraceMessages(bool value) {TraceMessages(value,""); }
    protected:
        virtual void OnChannelOpened() { }

        virtual CoTask<> OnEvent(const WpaEvent&event) { co_return;}
        std::shared_ptr<ILog> GetSharedLog() { return pLog; }

    private:
        WpaCtrl commandSocket;
        WpaCtrl eventSocket;
        CoMutex requestMutex;


        bool closed = true;
        bool withEvents = false;
        std::string interfaceName;

        bool traceMessages = false;
        std::string logPrefix;

        std::shared_ptr<ILog> pLog;
        ILog*rawLog = nullptr; // ILog is thread-safe shared_ptr<ILog> is not!

        bool disconnected = false;
        CoConditionVariable cvDelay;

        CoBlockingQueue<WpaEvent> eventMessageQueue { 512};

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

        CoTask<> ReadEventsProc(std::string interfaceName);
        CoTask<> ForegroundEventHandler();



        void SetDisconnected();

        char requestReplyBuffer[4096]; // the maximum length of a UNIX socket buffer.

    };


} // namespace


