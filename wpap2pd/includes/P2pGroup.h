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
