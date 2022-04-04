
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

        // dynamic p2p configuration options 
        std::string p2p_config_methods = "keypad";
        std::string p2p_device_name = "PiPedal";  // Max 18 characters, gives DIRECT-baseSid-XXX devicename, and DIRECT-XX-BaseSid-xxx names.
        std::string p2p_manufacturer = "The PiPedal Project";
        std::string p2p_model_name = "PiPedal";
        std::string p2p_model_number = "1";
        std::string p2p_default_pin = "12345678";
        std::string p2p_serial_number = "1";
        std::string p2p_device_type = "1-0050F204-1";
        std::string p2p_os_version = "";
        bool p2p_go_ht40 = false;
        std::string p2p_sec_device_type = "";
        uint8_t p2p_go_intent = 15;
        std::string p2p_ssid_postfix = ""; // defaults to device_name;
        bool persistent_reconnect = true;




    };

    extern P2pConfiguration gP2pConfiguration;
}
