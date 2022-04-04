#include "includes/WpaEvent.h"

#include "ss.h"
#include "stddef.h"
#include <cctype>
#include <iomanip>

using namespace p2p;

bool skipBalancedPair(const char *line, const char **lineOut)
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
        this->messageString = message;
    }
    else
    {
        this->messageString.resize(0);
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

        if (skipBalancedPair(line, &line))
        {
            this->parameters.push_back(std::string(pStart, line));
        }
        else
        {
            const char *pEquals = nullptr;
            while (*line != ' ' && *line != '\0')
            {
                if (*line == '=')
                {
                    pEquals = line;
                    ++line;
                    if (skipBalancedPair(line, &line))
                    {
                        break;
                    }
                }
                else
                {
                    ++line;
                }
            }
            if (pEquals)
            {
                std::pair pair(std::string(pStart, pEquals), std::string(pEquals + 1, line));
                this->namedParameters.push_back(std::move(pair));
            }
            else
            {
                this->parameters.push_back(std::string(pStart, line));
            }
        }
    }
    return true;
}

static std::string emptyString;

const std::string &WpaEvent::GetNamedParameter(const std::string &name) const
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

std::string WpaEvent::QuoteString(const std::string &value, char quoteChar)
{
    std::stringstream s;
    const char *p = value.c_str();
    s << quoteChar;
    while (*p)
    {
        char c = *p++;
        if (c == quoteChar || c == '\\')
        {
            s << '\\';
        }
        s << c;
    }
    s << quoteChar;
    return s.str();
}

std::string WpaEvent::UnquoteString(const std::string &value)
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

    ++p;
    while (*p != 0 && *p != quoteChar)
    {
        char c = *p++;
        if (c == '\\')
        {
            c = *p++;
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

std::string WpaEvent::ToString()
{
    std::stringstream s;
    s << '<' << (char)('0' + (int)priority) << '>';
    if (this->message == WpaEventMessage::WPA_UNKOWN_MESSAGE)
    {
        s << this->messageString;
    }
    else
    {
        s << WpaEventMessageToString(this->message);
    }
    for (size_t i = 0; i < this->parameters.size(); ++i)
    {
        s << " " << this->parameters[i];
    }
    for (auto i = this->namedParameters.begin(); i != this->namedParameters.end(); ++i)
    {
        s << " " << i->first << '=' << i->second;
    }
    return s.str();
}

std::string WpaEvent::ToIntLiteral(uint64_t value)
{
    std::stringstream s;
    s << value;
    return s.str();
}
std::string WpaEvent::ToIntLiteral(int64_t value)
{
    std::stringstream s;
    s << value;
    return s.str();
}
std::string WpaEvent::ToHexLiteral(uint64_t value)
{
    std::stringstream s;
    s << "0x" << std::setbase(16) << value;
    return s.str();
}
std::string WpaEvent::ToHexLiteral(uint32_t value)
{
    std::stringstream s;
    s << "0x" << std::setbase(16) << value;
    return s.str();
}
std::string WpaEvent::ToHexLiteral(uint16_t value)
{
    std::stringstream s;
    s << "0x" << std::setbase(16) << value;
    return s.str();
}

std::string WpaEvent::ToHexLiteral(uint8_t value)
{
    std::stringstream s;
    s << "0x" << std::setbase(16) << value;
    return s.str();
}
