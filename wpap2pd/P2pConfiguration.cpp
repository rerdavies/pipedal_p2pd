#include "includes/P2pConfiguration.h"
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

        SERIALIZER_ENTRY(p2p_model_name,"P2P Device info"),
        SERIALIZER_ENTRY(p2p_model_number,""),
        SERIALIZER_ENTRY(p2p_manufacturer,""),
        SERIALIZER_ENTRY(p2p_serial_number,""),
        SERIALIZER_ENTRY(p2p_device_type,""),

        SERIALIZER_ENTRY(wlanInterface, "Wi-Fi configuration"),
        SERIALIZER_ENTRY(p2pInterface, ""),
        SERIALIZER_ENTRY(p2p_go_ht40, ""),
        SERIALIZER_ENTRY(p2p_go_vht, ""),
        SERIALIZER_ENTRY(p2p_go_he, ""),
        SERIALIZER_ENTRY(p2p_ip_address, "Ipv4 address for the P2P group interface"),
        SERIALIZER_ENTRY(service_guid_file, 
            "File containing the a globally-unique id identifying the service on this machine.\n"
            "Syntax: 0a6045b0-1753-4104-b3e4-b9713b9cc356\n"),
        SERIALIZER_ENTRY(service_guid, 
            "GUID identifying the PiPedal service\n"
            "(if service_guid_file is not provided.)"
            ),
        SERIALIZER_ENTRY(p2p_device_type, ""),

};

void P2pConfiguration::MakeUuid()
{
    if (this->service_guid == "")
    {
        this->service_guid = os::MakeUuid();
    }
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
    if (start >= end)
        return "";
    return v.substr(start, end - start);
}

void P2pConfiguration::Load(const std::filesystem::path &path)
{
    std::unordered_map<std::string, ConfigSerializerBase *> index;

    for (ConfigSerializerBase *serializer : configSerializers)
    {
        index[serializer->GetName()] = serializer;
    }

    std::ifstream f{path};
    if (f.is_open())
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
        size_t start = line.find_first_of(' ');
        if (start == line.size())
        {
            continue;
        }
        auto pos = line.find_first_of('=', start);
        if (pos == std::string::npos)
        {
            std::string message = SS(
                "Error " << path.string() << "(" << line << ',' << (pos + 1) << "): Syntax error. Expecting '='.");
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
                        "Error " << path.string() << "(" << line << ',' << (pos + 1) << "): " << e.what());
                throw logic_error(message);
            }
        }
    }
    if (this->service_guid_file != "")
    {
        ifstream f;
        f.open(this->service_guid_file);
        if (f.fail())
        {
            throw invalid_argument(
                SS("Can't open file " << this->service_guid_file)
            );
        }
        f >> this->service_guid;
    }
    if (this->service_guid == "")
    {
        MakeUuid();
        Save(path);
    }
}
