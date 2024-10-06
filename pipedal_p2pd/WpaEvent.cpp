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

#include "includes/WpaEvent.h"

#include "ss.h"
#include <stddef.h>
#include <cctype>
#include <iomanip>
#include <string.h>

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
    this->parameters.resize(0);
    this->namedParameters.resize(0);
    this->messageString.resize(0);  

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
    ++line;

    this->priority = (EventPriority)priorityLevel;

    if (strncmp(line,"CTRL-REQ-",strlen("CTRL-REQ-")) == 0)
    {
        this->message = WpaEventMessage::WPA_CTRL_REQ;
        this->parameters.push_back(line);
        return true;
    }
    if (strncmp(line,"CTRL-RSP-",strlen("CTRL-RSP-")) == 0)
    {
        this->message = WpaEventMessage::WPA_CTRL_RSP;
        this->parameters.push_back(line);
        return true;
    }

    pStart = line;
    while (*line != ' ' && *line != '\0')
    {
        ++line;
    }
    std::string message{pStart, (size_t)(line - pStart)};



    WpaEventMessage wpaMessage = GetWpaEventMessage(message);
    if (wpaMessage == WpaEventMessage::WPA_UNKOWN_MESSAGE
        || wpaMessage == WpaEventMessage::FAIL)
    {
        this->messageString = message;
    }
    this->message = wpaMessage;



    if (this->message == WpaEventMessage::WPA_P2P_INFO)
    {
        while (*line == ' ')
        {
            ++line;
        }
        this->parameters.push_back(line);
        return true;
    }

    while (true)
    {
        while (*line == ' ')
        {
            ++line;
        }
        if (*line == '\0')
            break;

        pStart = line;

        if (*line == '[')
        {
            // parse options [ | | |xxx]
            ++line;
            while (*line  != ']' && *line != '\0')
            {                pStart = line;
                while (*line != '|' && *line != ']' && *line != '\0')
                {
                    ++line;
                }
                const char *pEnd = line;
                if (pEnd[-1] == ' ')
                {
                    --pEnd;
                }
                options.push_back(std::string(pStart,pEnd));
                if (*line == '|') ++line;

            }
            if (*line == ']') ++ line;
            while (*line == ' ') ++line;

        }
        else if (skipBalancedPair(line, &line))
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

std::string WpaEvent::ToString() const
{
    std::stringstream s;
    s << '<' << (char)('0' + (int)priority) << '>';
    if (this->message == WpaEventMessage::WPA_UNKOWN_MESSAGE
    || this->message == WpaEventMessage::FAIL)
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

    if (options.size() != 0)
    {
        s << '[';
        bool first = true;
        for (const std::string &option: options)
        {
            if (!first)
            {
                s << " |";
            }
            first = false;
            s << option;
        }
        s << ']';
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
