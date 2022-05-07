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

#include "includes/P2pUtil.h"
#include <random>
#include <sstream>

using namespace p2p;

using namespace std;

uint64_t p2p::toUint64(const std::string &value)
{
    const char *p = value.c_str();

    if (*p == '0' && (p[1] == 'X' || p[1] == 'x'))
    {
        p += 2;
        uint64_t value = 0;
        while (*p)
        {
            char c = *p++;
            int digit;
            if (c >= '0' && c <= '9')
            {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = c - 'A' + 10;
            }
            else
            {
                throw invalid_argument("Invalid number.");
            }
            value = value * 16 + digit;
        }
        return value;
    }
    else
    {
        // decimal
        uint64_t value = 0;
        if (!*p)
        {
            throw invalid_argument("Invalid number.");
        }
        while (isdigit(*p))
        {
            value = value * 10 + (*p++) - '0';
        }
        if (*p != 0)
        {
            throw invalid_argument("Invalid number.");
        }
        return value;
    }
}

int64_t p2p::toInt64(const std::string &value)
{
    const char *p = value.c_str();
    int64_t sign = 1;
    if (*p == '+')
    {
        ++p;
    }
    else if (*p == '-')
    {
        sign = -1;
        ++p;
    }

    if (*p == '0' && (p[1] == 'X' || p[1] == 'x'))
    {
        p += 2;
        int64_t value = 0;
        while (*p)
        {
            char c = *p++;
            int digit;
            if (c >= '0' && c <= '9')
            {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = c - 'A' + 10;
            }
            else
            {
                throw invalid_argument("Invalid number.");
            }
            value = value * 16 + digit;
        }
        return sign * value;
    }
    else
    {
        // decimal
        int64_t value = 0;
        if (!*p)
        {
            throw invalid_argument("Invalid number.");
        }
        while (isdigit(*p))
        {
            value = value * 10 + (*p++) - '0';
        }
        if (*p != 0)
        {
            throw invalid_argument("Invalid number.");
        }
        return sign * value;
    }
}

std::vector<std::string> p2p::split(const std::string &value, char delimiter)
{
    size_t start = 0;
    std::vector<std::string> result;
    while (start < value.length())
    {
        size_t pos = value.find_first_of(delimiter, start);
        if (pos == std::string::npos)
        {
            result.push_back(value.substr(start));
            break;
        }
        result.push_back(value.substr(start, pos - start));
        start = pos + 1;
        if (start == value.length())
        {
            // ends with delimieter? Then there's an empty-length value at the end.
            result.push_back("");
        }
    }
    return result;
}
std::vector<std::string> p2p::splitWpaFlags(const std::string &value)
{
    size_t i = 0;
    size_t end = value.length();
    std::vector<std::string> result;
    while (i != end)
    {
        size_t start = i;
        ++i;
        while (i != end && value[i] != '[')
            ++i;
        result.push_back(value.substr(start, i - start));
    }
    return result;
}

static std::random_device randomDevice;

static std::uniform_int_distribution distribution{0, 26 * 2 + 10 - 1};

std::string p2p::randomText(size_t length)
{
    std::stringstream s;
    for (size_t i = 0; i < length; ++i)
    {
        int value = distribution(randomDevice);
        if (value < 26)
        {
            s << char('a' + value);
        }
        else
        {
            value -= 26;
            if (value < 26)
            {
                s << char('A' + value);
            }
            else
            {
                value -= 26;
                s << char('0' + value);
            }
        }
    }
    return s.str();
}

std::string p2p::EncodeString(const std::string &s)
{
    bool requiresEncoding = false;

    for (char c : s)
    {
        switch (c)
        {
        case '\0':
            break;
        case '\r':
        case '\t':
        case '\\':
        case ' ':
        case '\"':
            requiresEncoding = true;
            break;
        default:
            break;
        }
    }
    if (!requiresEncoding)
        return s;

    stringstream ss;

    ss << '"';
    for (char c : s)
    {
        switch (c)
        {
        case '\0':
            // discard.
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        case '\\':
            ss << "\\\\";
            break;
        case '\"':
            ss << "\\\"";
            break;
        default:
            ss << c;
        }
    }
    ss << '"';
    return ss.str();
}

std::string p2p::DecodeString(const std::string &value)
{
    std::stringstream s;
    const char *p = value.c_str();
    char quoteChar;
    if (*p == '\'' || *p == '\"')
    {
        quoteChar = *p;
    }
    else
    {
        return value;
    }
    if (*p != quoteChar)
        return value;
    if (value.at(value.length()-1) != quoteChar)
    {
        throw std::invalid_argument("Invalid quoted string.");
    }
    ++p;
    while (*p != 0 && *p != quoteChar)
    {
        char c = *p++;
        if (c == '\\')
        {
            c = *p++;
            switch (c)
            {
            case 'r':
                s << '\r';
                break;
            case 't':
                s << '\t';
                break;
            case 'n':
                s << '\n';
                break;
            case '\0':
                throw std::invalid_argument("Invalid quoted string.");
                break;
            default:
                s << c;
            }
            if (c != 0)
            {
                s << c;
            }
        }
        else
        {
            s << c;
        }
    }
    return s.str();
}


std::string p2p::toHex(uint8_t byteVal)
{
    return toHex(1,&byteVal);
}
static char hexChars[] = "0123456789abcdef";

std::string p2p::toHex(uint16_t val)
{
    stringstream s;
    for (int i = sizeof(val)-1; i >= 0; --i)
    {
        uint8_t v = (val >> (i*8)) & 0x0FF;
        s << hexChars[ (v >> 4) & 0x0F];
        s << hexChars[ (v) & 0x0F];
    }
    return s.str();
}




std::string p2p::toHex(size_t length, const void*pData)
{
    const uint8_t *p = (uint8_t*)pData;
    stringstream s;

    for (size_t i = 0; i < length; ++i)
    {
        uint8_t v = p[i];
        s << hexChars[(v >> 4) & 0x0F];
        s << hexChars[v & 0x0F];
    }
    return s.str();
}

std::string p2p::toHex(const std::vector<uint8_t> &data)
{
    return toHex(data.size(),&(data[0]));
}

char p2p::ansiToLower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c-'A'+'a');
    }
    return c;
}
char p2p::ansiToUpper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c-'a'+'A');
    }
    return c;
}

std::string p2p::ansiToLower(const std::string &value)
{
    stringstream result;
    for (char c: value)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c+'a'-'A');
        }
        result<< c;
    }
    return result.str();
}
std::string p2p::ansiToUpper(const std::string &value)
{
    stringstream result;
    for (char c: value)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c+'A'-'a');
        }
        result<< c;
    }
    return result.str();
}

bool p2p::caseInsensitiveCompare(const std::string &left, const std::string&right)
{
    if (left.length() != right.length()) return false;

    for (size_t i = 0; i < left.length(); ++i)
    {
        char cleft = ansiToLower(left.at(i));
        char cright = ansiToLower(right.at(i));
        if (cleft != cright) return false;
    }
    return true;

}

bool p2p::isValidDnsSdName(const std::string &value)
{
    // http://files.dns-sd.org/draft-cheshire-dnsext-dns-sd.txt
    // < 64 characters, control codes (0x0-0x1F and 0x7F) are illegal, otherwise any valid (pre-composed) UTF-8 name.
    // Additionally, the rfc specifies optional(!) \-encoding for '."", so we will disallow '.' and '\'.
    if (value.length() >= 63) return false;
    for (char c: value)
    {
        uint8_t b = (uint8_t)c;
        if (b <= 0x1F) return false;
        if (b == 0x7F) return false;
        if (c == '.') return false;
        if (c == '\\') return false;
    }
    return true;

}