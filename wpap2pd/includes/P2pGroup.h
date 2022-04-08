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

        void OpenChannel();
    protected:
        void SetP2pProperty(const std::string &name, const std::string &value);


    public:
        P2pGroup();
        P2pGroup(P2pSessionManager*pSessionManager,const  P2pGroupInfo &groupInfo)
        : pSessionManager(pSessionManager),
            groupInfo(groupInfo)
        {
              
        }
        const P2pGroupInfo &GroupInfo() const { return groupInfo; }
    private:
        void PreAuth();
        virtual void OnEvent(const WpaEvent &event);
        void SetParameters();
        P2pSessionManager *pSessionManager = nullptr;
        P2pGroupInfo groupInfo;

    };

} // namespace
