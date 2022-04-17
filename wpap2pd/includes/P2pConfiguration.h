
#pragma once
#include <chrono>
#include <string>
#include <filesystem>
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
        bool runDnsMasq=false;
            const std::string dhcpLeaseFilePath = "/home/pi/var/dnsmasq_leases.db";
            const std::string dhcpConfFile = "/home/pi/var/p2p-dnsmasq.conf";


        uint32_t  wifiGroupFrequency = 2412; // ch 1 by default. Should be 1, 6, or 11.
        int randomSuffixChars = 2;
        std::string country_code = "CA";
        std::string wlanInterface = "wlan0";
        std::string p2pInterface = "p2p-dev-wlan0";
        seconds initialP2pFindTime = 60s; // length of initial p2p_find
        seconds p2pFindRefreshInterval = 600s; 
        seconds refresnP2pFindTime = 10s; 
        bool updateConfig = true;     // update wpa_supplement config files?
        std::string p2p_pin = "12345678";
        std::string p2p_device_name = "PiPedal";
        std::string p2p_ssid_postfix = "PiPedalGroup"; 
        std::string p2p_ip_address = "172.24.0.2/16";
        bool p2p_per_sta_psk = false;
        int p2p_go_intent = 15;
        bool p2p_go_ht40 = false;
        bool p2p_go_vht = false;
        bool p2p_go_he = false;

        // Global p2p configuration options 
        std::string p2p_model_name = "PiPedal";
        std::string p2p_model_number = "1";
        std::string p2p_manufacturer = "The PiPedal Project";
        std::string p2p_serial_number = "1";
        std::string p2p_device_type = "1-0050F204-1";
        std::string p2p_os_version = "";
        std::string p2p_sec_device_type = "";
        std::string p2p_config_method = "label"; // keypad, label, display, pbc, or none.
        bool persistent_reconnect = true;

        std::string service_guid_file = "";
        std::string service_guid = "0a6045b0-1753-4104-b3e4-b9713b9cc356";
        P2pGroupConfiguration defaultGroupConfiguration;


        std::filesystem::path path;
        void MakeUuid();
        void Save() { Save(path); }
        void Save(const std::filesystem::path &path);
        void Load(const std::filesystem::path &path);

    };

    extern P2pConfiguration gP2pConfiguration;
}
