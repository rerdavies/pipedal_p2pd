#pragma once

#include "WpaSupplicant.h"
#include "P2pGroup.h"
#include <unordered_map>
#include "DnsMasqProcess.h"

namespace p2p
{
    
    constexpr size_t WPS_DEV_NAME_LEN = 32;
    constexpr size_t WPS_MANUFACTURER_MAX_LEN = 64;
    constexpr size_t WPS_MODEL_NAME_MAX_LEN = 32;
    constexpr size_t WPS_SERIAL_NUMBER_MAX_LEN = 32;

    struct P2pDeviceInfo
    {
        P2pDeviceInfo() {}
        P2pDeviceInfo(const WpaEvent &event);
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

    class P2pGroup;

    class P2pSessionManager : public WpaSupplicant
    {
    public:
        P2pSessionManager();
        using base = WpaSupplicant;
        const std::list<P2pDeviceInfo> &GetDevices() const { return devices; }

        virtual void SetLog(std::shared_ptr<ILog> log);
        CoTask<> Open(const std::string &interfaceName);
        
    protected:

        struct  EnrollmentRecord {
            bool active = false;
            bool pbc = false;
            std::string pin;
            std::string deviceId;

        };
        EnrollmentRecord enrollmentRecord;
        std::mutex mxEnrollmentRecord;

        virtual CoTask<> InitWpaConfig();
        virtual CoTask<>  SetUpPersistentGroup();
        virtual CoTask<> OnEvent(const WpaEvent &event);
        virtual CoTask<> CoOnInit();
        virtual CoTask<> CoOnUnInit();
        virtual CoTask<> MaybeEnableNetwork(const std::vector<WpaNetworkInfo> &networks);
        virtual CoTask<> AttachDnsMasq();
        virtual CoTask<> OnEndOfEnrollment(bool hasStations) { co_return; }

        // P2p event handlers.
        virtual CoTask<> OnP2pGoNegRequest(const P2pGoNegRequest &request);
        virtual CoTask<> OnProvDiscShowPin(const WpaEvent &event);
        CoTask<> OnProvDiscPbcReq(const WpaEvent &event);

        CoTask<> OnGroupStarted(const P2pGroupInfo &groupInfo);
        CoTask<> OnGroupRemoved(const WpaEvent&event);
        CoTask<> SetP2pProperty(const std::string &name, const std::string &value);

    private:
        CoTask<> CloseGroup(P2pGroup *group);
        CoTask<> EndEnrollment();
        CoTask<> UpdateStationCount();
        std::string interfaceName;
        DnsMasqProcess dnsMasqProcess;
        CoTask<> CreateP2pGroup(const std::string &interfaceName);
        friend class P2pGroup;
        
        uint32_t connectedStations = 0;
        
        CoTask<> RemoveExistingGroups();
        
        CoTask<> StartServiceDiscovery();
        CoTask<> StopServiceDiscovery();
        CoTask<> ScanProc();

        virtual CoTask<> OpenChannel(const std::string &interfaceName,bool withEvents);
        CoTask<int> FindNetwork();
        int networkId;
        std::string networkBsid;
        std::vector<std::unique_ptr<P2pGroup>> activeGroups;

        bool addingGroup = false;
        WpaEventMessage sychronousMessageToWaitFor = WpaEventMessage::WPA_INVALID_MESSAGE;
        void SynchronousWaitForMessage(WpaEventMessage message, std::chrono::milliseconds timeout);

        int persistentGroup = -1;
        bool wpaConfigChanged = false;
        std::list<P2pDeviceInfo> devices;
        int myNetwork = -1;

        std::unordered_map<std::string,std::string> bsidToNameMap;
        std::vector<WpaNetworkInfo> networks;
        CoTask<> OnDeviceFound(P2pDeviceInfo &&device); 
        CoTask<> OnDeviceLost(std::string p2p_dev_addr);
        std::vector<WpaScanInfo> scanResults;
    };
}

// <3>P2P-GROUP-STARTED p2p-wlan0-3 GO ssid="DIRECT-NFGroup" freq=5180 passphrase="Z6qktTv6" go_dev_addr=de:a6:32:d4:a1:a4
