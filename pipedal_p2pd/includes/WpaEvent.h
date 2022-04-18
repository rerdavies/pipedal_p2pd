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


#pragma once

#include "WpaMessages.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <concepts>
#include "P2pUtil.h"
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
        std::string messageString; // only for WpaEventMessage::WPA_UNKOWN_MESSAGE and WpaEventMessage::FAIL messages.
        std::vector<std::string> parameters;
        std::vector<key_value_pair> namedParameters;
        std::vector<std::string> options;

        const std::string&GetNamedParameter(const std::string&name) const;
        template<std::integral  T> 
        T GetNumericParameter(const  std::string &name) const {
            try {
                return ToInt<T>(GetNamedParameter(name));
            } catch (const std::exception &e)
            {
                throw std::invalid_argument("Invalid property " +name +": " + e.what());
            }
        }
        const std::string getParameter(size_t index) const
        {
            if (index >= parameters.size()) return "";
            return parameters[index];
        }
        
        template<std::integral  T> 
        T GetNumericParameter(const  std::string &name, T defaultValue) const {
            try {
                return ToInt<T>(GetNamedParameter(name));
            } catch (const std::exception &e)
            {
                return defaultValue;
            }
        }
        
        bool ParseLine(const char *line);
        static std::string UnquoteString(const std::string & value);
        static std::string QuoteString(const std::string &value, char quoteChar = '\'');
        static std::string ToIntLiteral(uint64_t value);
        static std::string ToIntLiteral(int64_t value);
        template <typename T> 
        static std::string ToIntLiteral(T value)
        {
            return ToIntLiteral((int64_t)value);
        }
        static std::string ToHexLiteral(uint64_t value);
        static std::string ToHexLiteral(uint32_t value);
        static std::string ToHexLiteral(uint16_t value);
        static std::string ToHexLiteral(uint8_t value);
        std::string ToString() const;
    };

}