
#pragma once

#include "WpaMessages.h"
#include <string>
#include <vector>

namespace p2psession
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
        using key_value_pair = std::pair<std::string,std::string>;
        EventPriority priority;
        WpaEventMessage message;
        std::string messageString; // only for Unknown messags.
        std::vector<std::string> parameters;
        std::vector<key_value_pair> namedParameters;

        const std::string&GetNamedParameter(const std::string&name) const;
        
        int64_t GetNamedNumericParameter(const std::string & name,int64_t defaultValue = -1) const;

        bool ParseLine(const char *line);
    };

}