#pragma once

#include "WpaCliWrapper.h"

namespace p2p
{
    const size_t WPS_DEV_NAME_LEN = 32;
    const size_t WPS_MANUFACTURER_MAX_LEN = 64;
    const size_t WPS_MODEL_NAME_MAX_LEN = 32;
    const size_t WPS_SERIAL_NUMBER_MAX_LEN = 32;

    struct P2pGroup
    {
        P2pGroup() { }
        P2pGroup(const WpaEvent &event);
        std::string interface;
        bool go = false;
        std::string ssid;
        uint32_t freq = 0;
        std::string passphrase;
        std::string go_dev_addr;
    };
    struct P2pDevice
    {
        P2pDevice() {}
        P2pDevice(const WpaEvent &event);
        std::string address;
        std::string p2p_dev_addr;
        std::string pri_dev_type;
        std::string name;
        uint16_t config_methods = 0;
        uint8_t dev_capab = 0;
        uint8_t group_capab = 0;
        uint64_t wfd_dev_info = 0;
        uint32_t vendor_elems = 0;
        uint32_t new_ = 0;
    };

    struct P2pGoNegRequest
    {
        //<3>P2P-GO-NEG-REQUEST 8a:3b:2d:b6:9f:8e dev_passwd_id=1 go_intent=14
        P2pGoNegRequest(const WpaEvent &event);
        std::string src;
        uint16_t dev_passwd_id;
        uint8_t go_intent;
    };

    class P2pSessionManager : public WpaCliWrapper
    {
    public:
        using base = WpaCliWrapper;
        const std::list<P2pDevice> &GetDevices() const { return devices; }

    protected:
        virtual void InitWpaConfig();
        virtual void SetUpPersistentGroup();
        virtual void OnEvent(Source source, WpaEvent &event);
        virtual CoTask<> CoOnInit();
        virtual void OnDevicesChanged() {}
        virtual void MaybeCreateNetwork(const std::vector<WpaNetwork> &networks);
        virtual void MaybeEnableNetwork(const std::vector<WpaNetwork> &networks);

        virtual void OnP2pGoNegRequest(const P2pGoNegRequest &request);
        void OnGroupStarted(const WpaEvent&event);
        void OnGroupStopped(const WpaEvent&event);

    private:
        std::unique_ptr<P2pGroup> activeGroup;
        bool addingGroup = false;
        WpaEventMessage sychronousMessageToWaitFor = WpaEventMessage::WPA_INVALID_MESSAGE;
        void SynchronousWaitForMessage(WpaEventMessage message, std::chrono::milliseconds timeout);

        int persistentGroup = -1;
        bool wpaConfigChanged = false;
        std::list<P2pDevice> devices;
        int myNetwork = -1;
        std::vector<WpaNetwork> networks;
        void OnDeviceFound(P2pDevice &&device);
        void OnDeviceLost(std::string p2p_dev_addr);
        std::vector<WpaScanResult> scanResults;
    };
}

// <3>P2P-GROUP-STARTED p2p-wlan0-3 GO ssid="DIRECT-NFGroup" freq=5180 passphrase="Z6qktTv6" go_dev_addr=de:a6:32:d4:a1:a4
