#include "includes/P2pUtil.h"


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
                digit = c-'0';
            } else if (c >= 'a' && c <= 'f')
            {
                digit = c-'a'+10;
            } else if (c >= 'A' && c <= 'F')
            {
                digit = c-'A'+10;
            } else {
                throw invalid_argument("Invalid number.");
            }
            value = value*16 + digit;
        }
        return value;
    }  else {
        // decimal
        uint64_t value = 0;
        if (!*p)
        {
            throw invalid_argument("Invalid number.");
        }
        while (isdigit(*p))
        {
            value = value*10 + (*p)-'0';

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
    } else if (*p == '-')
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
                digit = c-'0';
            } else if (c >= 'a' && c <= 'f')
            {
                digit = c-'a'+10;
            } else if (c >= 'A' && c <= 'F')
            {
                digit = c-'A'+10;
            } else {
                throw invalid_argument("Invalid number.");
            }
            value = value*16 + digit;
        }
        return sign*value;
    }  else {
        // decimal
        int64_t value = 0;
        if (!*p)
        {
            throw invalid_argument("Invalid number.");
        }
        while (isdigit(*p))
        {
            value = value*10 + (*p)-'0';

        }
        if (*p != 0)
        {
            throw invalid_argument("Invalid number.");
        }
        return sign*value;

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
