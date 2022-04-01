#include "cotask/WpaEvent.h"

#include "ss.h"
#include "stddef.h"
#include <cctype>

using namespace cotask;

bool skipBalancedPair(const char *line,const char**lineOut)
{
    char terminatorCharacter = '\0';
    switch (*line)
    {
    case '\"':
        terminatorCharacter = '\"';
        break;
    case '\'':
        terminatorCharacter = '\'';
        break;
    case '[':
        terminatorCharacter = ']';
        break;
    }
    if (terminatorCharacter == '\0')
        return false;
    ++line;
    while (*line != terminatorCharacter && *line != 0)
    {
        ++line;
    }
    if (*line != 0)
        ++line;
    *lineOut = line;
    return true;
}
bool WpaEvent::ParseLine(const char *line)
{

    if (line[0] == '>')
    { // ignore prompt.
        ++line;
    }
    if (line[0] == '\0')
        return true;
    if (line[0] != '<')
        return false;

    WpaEvent event;

    ++line;
    const char *pStart = line;
    int priorityLevel = 0;
    while (isdigit(*line))
    {
        priorityLevel = priorityLevel * 10 + *line - '0';
        ++line;
    }
    if (*line != '>')
        return false;

    this->priority = (EventPriority)priorityLevel;

    ++line;
    pStart = line;
    while (*line != ' ' && *line != '\0')
    {
        ++line;
    }
    std::string message{pStart, (size_t)(line - pStart)};

    WpaEventMessage wpaMessage = GetWpaEventMessage(message);
    if (wpaMessage == WpaEventMessage::WPA_UNKOWN_MESSAGE)
    {
        event.messageString = message;
    } else {
        event.messageString.resize(0);
    }
    this->message = wpaMessage;

    this->parameters.resize(0);
    this->namedParameters.resize(0);

    while (true)
    {
        while (*line == ' ')
        {
            ++line;
        }
        if (*line == '\0')
            break;

        pStart = line;
        const char *pEquals = nullptr;

        if (skipBalancedPair(line,&line))
        {
            this->parameters.emplace_back(std::string(pStart,line));
        } else 
        {
            const char *pEquals = nullptr;
            while (*line != ' ' && *line != '\0')
            {
                if (*line == '=')
                {
                    pEquals = line;
                    ++line;
                    if (!skipBalancedPair(line,&line))
                    {
                        break;
                    }
                } else {
                    ++line;
                }
            }
            if (pEquals)
            {
                std::pair pair(std::string(pStart,pEquals),std::string(pEquals+1,line));
                this->namedParameters.emplace_back(std::move(pair));
            } else {
                this->parameters.emplace_back(std::string(pStart,line));
            }
        }
    }
    return true;
}

static std::string emptyString;

const std::string&WpaEvent::GetNamedParameter(const std::string&name) const
{
    for (size_t i = 0; i < namedParameters.size(); ++i)
    {
        if (namedParameters[i].first == name)
        {
            return namedParameters[i].second;
        }
    }
    return emptyString;
}

int64_t WpaEvent::GetNamedNumericParameter(const std::string & name,int64_t defaultValue) const
{
    const std::string&parameter = GetNamedParameter(name);
    const char *p = parameter.c_str();
    int64_t val;
    int64_t sign = 1;
    if (p[0] == '+')
    {
        ++p;
    } else if (p[0] == '-')
    {
        ++p;
        sign = 1;
    }

    if (p[0] == 0) return defaultValue;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        p += 2;
        while (*p) {
            int dx = 0;
            char c = *p;
            if (c >= '0' && c <= '9')
            {
                dx = c-'0';
            } else if (c >= 'A' && c <= 'F')
            {
                dx = 10 + c-'A';
            } else if (c >= 'a' && c <= 'f')
            {
                dx += 10 + c- 'a';
            } else {
                return defaultValue;
            }
            val = val*16+dx;
        }
        return val*sign;
    } else {
        while (isdigit(*p))
        {
            val = val*10 + *p-'0';
        }
        if (*p != 0)
        {
            return defaultValue;
        }
        return val*sign;
    }
}