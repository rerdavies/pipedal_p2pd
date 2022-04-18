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
#include <vector>
#include <string>
#include "WpaChannel.h"

namespace p2p {
    // forward declaration
    class P2pSessionManager;

    struct P2pGroupInfo
    {
        P2pGroupInfo() { }
        P2pGroupInfo(const WpaEvent &event);
        std::string interface;
        bool go = false;
        std::string ssid;
        uint32_t freq = 0;
        std::string passphrase;
        std::string go_dev_addr;
    };


    class P2pGroup: public WpaChannel {
    public:
        using base = WpaChannel;

        CoTask<> OpenChannel();
    protected:
        CoTask<> SetP2pProperty(const std::string &name, const std::string &value);


    public:
        P2pGroup();
        P2pGroup(P2pSessionManager*pSessionManager,const std::string&interfaceName_)
        : pSessionManager(pSessionManager),interfaceName(interfaceName_) { }

        P2pGroup(P2pSessionManager*pSessionManager,const  P2pGroupInfo &groupInfo)
        : pSessionManager(pSessionManager),
            interfaceName(groupInfo.interface)
        {
              
        }
        // const P2pGroupInfo &GroupInfo() const { return groupInfo; }
        const std::string &GetInterfaceName() const { return interfaceName; }
    private:
        /**
         * @brief Ask P2pSessionManager to close the group.
         * 
         * P2pSession manager will start a shutdown if it receives this message.
         * 
         * @return CoTask<> 
         */
        std::string currentEnrollee;
        CoTask<> TerminateGroup();
        void DebugHook();
        CoTask<> PingProc();
        CoTask<> PreAuth(const std::string & clientid);
        virtual CoTask<> OnEvent(const WpaEvent &event);
        void SetParameters();
        P2pSessionManager *pSessionManager = nullptr;
        std::string interfaceName;
        // P2pGroupInfo groupInfo;
        CoTask<> OnEnrolleeSeen(const WpaEvent &event);

    };

} // namespace
