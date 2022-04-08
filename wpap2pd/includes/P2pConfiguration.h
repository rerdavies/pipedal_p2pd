
#pragma once
#include <chrono>
#include <string>
namespace p2p
{
    using namespace std;
    using seconds = chrono::seconds;
    // enum class SelectTimeoutSecs
    // {
    //     Normal = 10,   // p2p find refreshes
    //     Connect = 90,  // p2p connect
    //     Long = 600,    // p2p find refreshes after exceeding self.max_scan_polling
    //     Enroller = 600 // Period used by the enroller.
    // };


    struct P2pGroupConfiguration {
        // see wpa_supplicant README-P2P  (e.g. https://web.mit.edu/freebsd/head/contrib/wpa/wpa_supplicant/README-P2P)
        bool dummy;
    };
    // Potential future configuration options.
    struct P2pConfiguration {
        int randomSuffixChars = 2;
        std::string country_code = "CA";
        std::string wlanInterface = "wlan0";
        std::string p2pInterface = "p2p-dev-wlan0";
        seconds initialP2pFindTime = 60s; // length of initial p2p_find
        seconds p2pFindRefreshInterval = 600s; 
        seconds refresnP2pFindTime = 10s; 
        bool updateConfig = true;     // update wpa_supplement config files?
        std::string p2p_pin = "90000201";
        std::string p2p_device_name = "PiPedal";
        std::string p2p_ssid_postfix = "PiPedalGroup"; 
        std::string model_name = "PiPedal";
        std::string model_number = "1";
        bool p2p_per_sta_psk = false;
        int p2p_go_intent = 15;
        bool p2p_go_ht40 = false;
        bool p2p_go_vht = false;
        bool p2p_go_he = false;

        // Global p2p configuration options 
        std::string manufacturer = "The PiPedal Project";
        std::string p2p_serial_number = "1";
        std::string p2p_device_type = "1-0050F204-1";
        std::string p2p_os_version = "";
        std::string p2p_sec_device_type = "";
        std::string p2p_config_method = "keypad"; // keypad, display, pbc, or none.
        bool persistent_reconnect = true;

        std::string upnpUuid = "";
        P2pGroupConfiguration defaultGroupConfiguration;



        void Save() { }
        void Load() { }


    };

    extern P2pConfiguration gP2pConfiguration;
}
