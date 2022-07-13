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

#include "includes/P2pConfiguration.h"
#include "ServiceConfiguration.hpp"
#include "cotask/Os.h"
#include "sstream"
#include <concepts>
#include <autoptr_vector.h>
#include "includes/P2pUtil.h"
#include <fstream>
#include "ss.h"
#include <unordered_map>
#include "ConfigSerializer.h"

using namespace p2p;
using namespace std;
using namespace cotask;
using namespace config_serializer;

P2pConfiguration p2p::gP2pConfiguration;



#define SERIALIZER_ENTRY(MEMBER_NAME, COMMENT) \
    new ConfigSerializer<P2pConfiguration,decltype(P2pConfiguration::MEMBER_NAME)>(#MEMBER_NAME, &P2pConfiguration::MEMBER_NAME, COMMENT)

static p2p::autoptr_vector<ConfigSerializerBase<P2pConfiguration>> configSerializers =
    {
        SERIALIZER_ENTRY(country_code, "WiFi regdomain 2-letter country code.\nsee: http://www.davros.org/misc/iso3166.txt"),
        SERIALIZER_ENTRY(p2p_pin, "keypad/label pin"),
        SERIALIZER_ENTRY(p2p_device_name, "Name that appears when you connect."),
        SERIALIZER_ENTRY(p2p_ssid_postfix, "DIRECT-XX-postfix (appears on Android p2p Group names)"),
        SERIALIZER_ENTRY(wifiGroupFrequency, "Wifi frequency (kHz).\nShould almost always be  2412 (ch1),2437 (ch6), or 2462 (ch11)."),
        SERIALIZER_ENTRY(wifiChannel,"Ui use only. wifiGroupFrequency is authoritative"),
        SERIALIZER_ENTRY(enabled,"Ui use only. Service state is authoritative"),
        SERIALIZER_ENTRY(p2p_model_name, "P2P Device info"),
        SERIALIZER_ENTRY(p2p_model_number, ""),
        SERIALIZER_ENTRY(p2p_manufacturer, ""),
        SERIALIZER_ENTRY(p2p_serial_number, ""),
        SERIALIZER_ENTRY(p2p_device_type, ""),

        SERIALIZER_ENTRY(wlanInterface, "Wi-Fi configuration"),
        SERIALIZER_ENTRY(p2pInterface, ""),
        SERIALIZER_ENTRY(p2p_go_ht40, ""),
        SERIALIZER_ENTRY(p2p_go_vht, ""),
        SERIALIZER_ENTRY(p2p_go_he, ""),
        SERIALIZER_ENTRY(service_guid_file,
                    "# File containing the globally-unique id that identifies the service on this machine\n"
                    " in this format: 0a6045b0-1753-4104-b3e4-b9713b9cc356"
        ),
        SERIALIZER_ENTRY(service_guid,
                         "GUID identifying the PiPedal service\n"
                         "(if service_guid_file is not provided.)"),
        SERIALIZER_ENTRY(server_port,
            "Web server port number (loaded from server.conf if present."),
};

P2pConfiguration::P2pConfiguration()
:base(configSerializers)
{

}
void P2pConfiguration::Save(std::ostream&f)
{
    base::Save(f);
}



void P2pConfiguration::Load(const std::filesystem::path &path)
{
    base::Load(path.string(),false);

    pipedal::ServiceConfiguration serviceConfiguration;
    serviceConfiguration.Load();

    this->service_guid = serviceConfiguration.uuid;
    this->p2p_device_name = serviceConfiguration.deviceName;
    this->p2p_ssid_postfix = serviceConfiguration.deviceName;
    this->server_port = serviceConfiguration.server_port;

    if (this->service_guid == "")
    {
        throw std::logic_error("Service UUID not initializaed in " + std::string(pipedal::ServiceConfiguration::DEVICEID_FILE_NAME));
    }
}
