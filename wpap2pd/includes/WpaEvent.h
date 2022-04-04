
#pragma once

#include "WpaMessages.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace p2p
{

    // see https://w1.fi/wpa_supplicant/devel/ctrl_iface_page.html

    enum class EventPriority
    {
        MSGDUMP = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4
    };

    class WpaEvent
    {
    public:
        WpaEvent() { }

        using key_value_pair = std::pair<std::string,std::string>;
        EventPriority priority;
        WpaEventMessage message;
        std::string messageString; // only for Unknown messags.
        std::vector<std::string> parameters;
        std::vector<key_value_pair> namedParameters;

        const std::string&GetNamedParameter(const std::string&name) const;
        
        bool ParseLine(const char *line);
        static std::string UnquoteString(const std::string & value);
        static std::string QuoteString(const std::string &value, char quoteChar = '\'');
        static std::string ToIntLiteral(uint64_t value);
        static std::string ToIntLiteral(int64_t value);
        static std::string ToHexLiteral(uint64_t value);
        static std::string ToHexLiteral(uint32_t value);
        static std::string ToHexLiteral(uint16_t value);
        static std::string ToHexLiteral(uint8_t value);
        static std::string ToString();
    };

}