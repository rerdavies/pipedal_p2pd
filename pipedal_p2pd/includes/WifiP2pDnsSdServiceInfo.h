/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/***************************************************************************************************************************************************
Ported from https://android.googlesource.com/platform/frameworks/base/+/cdd0c59/wifi/java/android/net/wifi/p2p/nsd/WifiP2pDnsSdServiceInfo.java 
***************************************************************************************************************************************************/

#include <unordered_map>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "P2pUtil.h"
#include <stdexcept>

#include "DnsSdTxtRecord.h"

/**
 * A class for storing Bonjour service information that is advertised
 * over a Wi-Fi peer-to-peer setup.
 *
 * {@see android.net.wifi.p2p.WifiP2pManager#addLocalService}
 * {@see android.net.wifi.p2p.WifiP2pManager#removeLocalService}
 * {@see WifiP2pServiceInfo}
 * {@see WifiP2pUpnpServiceInfo}
 */

namespace p2p {
    using namespace std;

class WifiP2pDnsSdServiceInfo {
    /**
     * Bonjour version 1.
     * @hide
     */
public:
    static constexpr int VERSION_1 = 0x01;
    /**
     * Pointer record.
     * @hide
     */
    static constexpr int DNS_TYPE_PTR = 12;
    /**
     * Text record.
     * @hide
     */
    static constexpr int DNS_TYPE_TXT = 16;
private:
    /**
     * virtual memory packet.
     * see E.3 of the Wi-Fi Direct technical specification for the detail.<br>
     * Key: domain name Value: pointer address.<br>
     */
    static std::unordered_map<std::string, std::string> sVmPacket;
    /**
     * This constructor is only used in newInstance().
     *
     * @param queryList
     */
    std::vector<std::string> queryList;

    WifiP2pDnsSdServiceInfo(std::vector<std::string> queryList) 
    :queryList(queryList)
    {
    }
    /**
     * Create a Bonjour service information object.
     *
     * @param instanceName instance name.<br>
     *  e.g) "MyPrinter"
     * @param serviceType service type.<br>
     *  e.g) "_ipp._tcp"
     * @param txtMap TXT record with key/value pair in a map confirming to format defined at
     * http://files.dns-sd.org/draft-cheshire-dnsext-dns-sd.txt
     * @return Bonjour service information object
     */
public:
    WifiP2pDnsSdServiceInfo(std::string instanceName,
            std::string serviceType, std::vector<std::pair<std::string, std::string>> txtRecords) 
    {

        DnsSdTxtRecord txtRecord;

        for (auto pair: txtRecords) {
            txtRecord.set(pair.first, pair.second);
        }
        this->queryList.push_back(createPtrServiceQuery(instanceName, serviceType));
        this->queryList.push_back(createTxtServiceQuery(instanceName, serviceType, txtRecord));
    }

    const std::vector<std::string> &GetQueryList() const { return  queryList; }


    /**
     * Create wpa_supplicant service query for PTR record.
     *
     * @param instanceName instance name.<br>
     *  e.g) "MyPrinter"
     * @param serviceType service type.<br>
     *  e.g) "_ipp._tcp"
     * @return wpa_supplicant service query.
     */
    static std::string createPtrServiceQuery(std::string instanceName,
            std::string serviceType) {
        stringstream sb;
        sb << ("bonjour ");
        sb << (createRequest(serviceType + ".local.", DNS_TYPE_PTR, VERSION_1));
        sb << (" ");



        sb << toHex((uint8_t)instanceName.length());
        sb << toHex(instanceName.length(),(uint8_t*)instanceName.c_str());
        // This is the start point of this response.
        // Therefore, it indicates the request domain name.
        sb << ("c027");
        return sb.str();
    }
    /**
     * Create wpa_supplicant service query for TXT record.
     *
     * @param instanceName instance name.<br>
     *  e.g) "MyPrinter"
     * @param serviceType service type.<br>
     *  e.g) "_ipp._tcp"
     * @param txtRecord TXT record.<br>
     * @return wpa_supplicant service query.
     */
    static std::string createTxtServiceQuery(const std::string& instanceName,
            const std::string& serviceType,
            const DnsSdTxtRecord& txtRecord) {
        
        stringstream sb;
        sb << ("bonjour ");
            // is  ansiToLower required by standard? Android seems to prefer it.
        sb << (createRequest((ansiToLower(instanceName) + "." + serviceType + ".local."),
                DNS_TYPE_TXT, VERSION_1));
        sb << (" ");
        std::vector<uint8_t> rawData = txtRecord.getRawBytes();
        if (rawData.size() == 0) {
            sb << ("00");
        } else {
            sb << (toHex(rawData));
        }
        return sb.str();
    }
    /**
     * Create bonjour service discovery request.
     *
     * @param dnsName dns name
     * @param dnsType dns type
     * @param version version number
     * @hide
     */
    static std::string createRequest(std::string dnsName, int dnsType, int version) {
        stringstream sb;
        /*
         * The request format is as follows.
         * ________________________________________________
         * |  Encoded and Compressed dns name (variable)  |
         * ________________________________________________
         * |   Type (2)           | Version (1) |
         */
        sb << (compressDnsName(dnsName));
        sb << (toHex((uint16_t)dnsType));
        sb << (toHex((uint8_t)version));
        return sb.str();
    }
    /**
     * Compress DNS data.
     *
     * see E.3 of the Wi-Fi Direct technical specification for the detail.
     *
     * @param dnsName dns name
     * @return compressed dns name
     */
    static std::string compressDnsName(std::string dnsName) {
        stringstream sb;

        // The domain name is replaced with a pointer to a prior
        // occurrence of the same name in virtual memory packet.
        while (true) {
            if (sVmPacket.contains(dnsName))
            {
                sb << (sVmPacket[dnsName]);
                break;
            }
            size_t i = dnsName.find_first_of('.');
            if (i == std::string::npos) {
                if (dnsName.length() > 0) {
                    if (dnsName.length() >= 256) 
                        throw std::invalid_argument("DNS Name is malformed.");
                    sb <<  toHex((uint8_t)dnsName.length());
                    sb << toHex(dnsName.length(),dnsName.c_str());
                }
                // for a sequence of labels ending in a zero octet
                sb << ("00");
                break;
            }
            std::string name = dnsName.substr(0, i);
            dnsName = dnsName.substr(i + 1);
            if (name.length() >= 256) 
                throw std::invalid_argument("DNS Name is malformed.");
            sb << toHex((uint8_t)name.length());
            sb << toHex(name.length(),name.c_str());
        }
        return sb.str();
    }
};
}