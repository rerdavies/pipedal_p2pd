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
#include "DeviceIdFile.hpp"
#include "cotask/Os.h"
#include "sstream"
#include <concepts>
#include <static_vector.h>
#include "includes/P2pUtil.h"
#include <fstream>
#include "ss.h"
#include <unordered_map>

using namespace p2p;
using namespace std;
using namespace cotask;

P2pConfiguration p2p::gP2pConfiguration;

namespace p2p
{
    namespace detail
    {
        class ConfigSerializerBase
        {
        protected:
            ConfigSerializerBase(const std::string &name, const std::string &comment)
                : name(name),
                  comment(comment)
            {
            }

        public:
            virtual ~ConfigSerializerBase();

            const std::string &GetName() const { return name; }
            const std::string &GetComment() const { return comment; }
            virtual void SetValue(P2pConfiguration &config, const std::string &value) const = 0;
            virtual const std::string GetValue(P2pConfiguration &config) const = 0;

        private:
            std::string name;
            std::string comment;
        };

        template <class T>
        class ConfigSerializer : public ConfigSerializerBase
        {
        public:
            using pointer_type = T P2pConfiguration::*;
            using base = ConfigSerializerBase;

            ConfigSerializer(const std::string &name,
                             pointer_type pMember,
                             const std::string &comment)
                : base(name, comment),
                  pMember(pMember)
            {
            }
            virtual void SetValue(P2pConfiguration &config, const std::string &value) const
            {
                stringstream s(value);
                s >> (config.*pMember);
            }
            virtual const std::string GetValue(P2pConfiguration &config) const
            {
                stringstream s;
                s << config.*pMember;
                return s.str();
            }

        private:
            pointer_type pMember;
        };


        // string specializations.
        template <>
        void ConfigSerializer<std::string>::SetValue(P2pConfiguration &config, const std::string &value) const
        {
            config.*pMember = DecodeString(value);
        }
        template <>
        const std::string ConfigSerializer<std::string>::GetValue(P2pConfiguration &config) const
        {
            return EncodeString(config.*pMember);
        }
        // bool specializations.
        template <>
        void ConfigSerializer<bool>::SetValue(P2pConfiguration &config, const std::string &value) const
        {
            bool result = false;
            if (value == "")
            {
                result = false;
            } else if (value == "true")
            {
                result = true;
            } else if (value == "false")
            {
                result = false;
            } else {
                try {
                    int t = ToInt<int>(value);
                    result = (t != 0);
                } catch (const std::exception &)
                {
                    throw invalid_argument("Expecting true or false.");
                }
            }
            config.*pMember = result;
        }
        template <>
        const std::string ConfigSerializer<bool>::GetValue(P2pConfiguration &config) const
        {
            return (config.*pMember) ? "true": "false";
        }

    }
}

using namespace p2p::detail;

ConfigSerializerBase::~ConfigSerializerBase()
{
}

#define SERIALIZER_ENTRY(MEMBER_NAME, COMMENT) \
    new ConfigSerializer<decltype(P2pConfiguration::MEMBER_NAME)>(#MEMBER_NAME, &P2pConfiguration::MEMBER_NAME, COMMENT)

p2p::static_vector<ConfigSerializerBase> configSerializers =
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
};

/**
 * @brief Create a new Gservice guid.
 * 
 * @return true if a write of the config file is required.
 * @return false if no write is required.
 */
bool P2pConfiguration::MakeUuid()
{
    if (this->service_guid_file != "")
    {
        std::string uuid;
        for (int retry = 0; /**/; ++retry)
        {
            try
            {
                ifstream f;
                f.open(this->service_guid_file);
                if (f.fail())
                {
                    throw logic_error("Can't read from " + this->service_guid_file);
                }
                f >> uuid;
                this->service_guid = uuid;
                return false; // success!
            }
            catch (const std::exception &e)
            {
            }
            if (uuid == "")
            {
                uuid = os::MakeUuid();
            }
            std::ofstream f;
            f.open(this->service_guid_file,ios_base::trunc);
            if (f.fail())
            {
                if (retry == 5)
                {
                    throw logic_error("Can't write to " + this->service_guid_file);
                }
                os::msleep(500);
            } else {
                f << uuid << endl;
            }
        }
    }
    else if (this->service_guid == "")
    {
        this->service_guid = os::MakeUuid();
        return true;
    }
    return false;
}
void P2pConfiguration::Save(const std::filesystem::path &path)
{
    MakeUuid();

    std::ofstream f;
    f.open(path, ios_base::trunc);
    if (f.fail())
    {
        throw invalid_argument("Can't open " + path.string());
    }
    Save(f);
}
void P2pConfiguration::Save(std::ostream&f)
{
    MakeUuid();

    bool firstLine = true;
    for (ConfigSerializerBase *serializer : configSerializers)
    {
        if (serializer->GetComment().length() != 0)
        {
            if (!firstLine)
            {
                f << endl;
            }
            firstLine = false;
            std::vector<std::string> comments = split(serializer->GetComment(), '\n');

            if (comments.size() != 0)
                for (const std::string &comment : comments)
                {
                    f << "# " << comment << endl;
                }
        }

        f << serializer->GetName() << '=' << serializer->GetValue(*this) << endl;
    }
}

static std::string trim(const std::string &v)
{
    size_t start = v.find_first_not_of(' ');
    size_t end = v.find_last_not_of(' ');
    if (start >= end+1)
        return "";
    return v.substr(start, end+1 - start);
}

void P2pConfiguration::Load(const std::filesystem::path &path)
{
    std::unordered_map<std::string, ConfigSerializerBase *> index;

    for (ConfigSerializerBase *serializer : configSerializers)
    {
        index[serializer->GetName()] = serializer;
    }

    std::ifstream f{path};
    if (!f.is_open())
    {
        throw std::invalid_argument("Can't open file " + path.string());
    }
    std::string line;
    int nLine = 0;
    while (true)
    {
        if (f.eof())
            break;
        getline(f, line);
        ++nLine;

        // remove comments.
        auto npos = line.find_first_of('#');
        if (npos != std::string::npos)
        {
            line = line.substr(0, npos);
        }
        size_t start = 0;
        while (start < line.size() && line[start] == ' ')
        {
            ++start;
        }
        if (start == line.size())
        {
            continue;
        }

        auto pos = line.find_first_of('=', start);
        if (pos == std::string::npos)
        {
            std::string message = SS(
                "Error " << path.string() << "(" << nLine << "," << (start + 1) << "): Syntax error. Expecting '='.");
            throw logic_error(message);
        }
        string label = trim(line.substr(start, pos - start));
        string value = trim(line.substr(pos + 1));

        if (index.contains(label))
        {
            ConfigSerializerBase *serializer = index[label];
            try
            {
                serializer->SetValue(*this, value);
            }
            catch (const std::exception &e)
            {
                std::string message =
                    SS(
                        "Error " << path.string() << "(" << nLine << ',' << (pos + 1) << "): " << e.what());
                throw logic_error(message);
            }
        } else {
            throw logic_error(
                SS (
                    "Error " << path.string() << "(" << nLine << ',' << (start) << "): " << "Invalid property: " + label )
            );
        }
    }
    if (this->service_guid_file != "")
    {
        pipedal::DeviceIdFile deviceIdFile;
        deviceIdFile.Load();

        this->service_guid = deviceIdFile.uuid;
        this->p2p_device_name = deviceIdFile.deviceName;
        this->p2p_ssid_postfix = deviceIdFile.deviceName;
    } else {
        if (MakeUuid())
        {
            Save(path);
        }
    }
}
