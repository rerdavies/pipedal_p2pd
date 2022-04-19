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
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace p2p
{

    class PrettyPrinter
    {
    private:
        std::ostream &s;
        size_t lineWidth = 80;
        std::vector<char> lineBuffer;
        size_t column = 0;
        size_t indent = 0;
        size_t tabSize = 4;

    public:
        PrettyPrinter()
            : s(std::cout)
        {
            lineBuffer.resize(lineWidth * 2);
        }

        void Indent(size_t indent)
        {
            this->indent = indent;
        }

        void WriteLine() {
            lineBuffer.push_back('\n');
            s.write(&lineBuffer[0],lineBuffer.size());
            lineBuffer.clear();
            column = 0;
        }

        void HangingIndent(const std::string &text)
        {
            for (char c : text)
            {
                if (c == '\n') {
                    WriteLine();
                } else if (c == '\t') {
                    lineBuffer.push_back(' ');
                    ++column;
                    while ((column % tabSize) != 0)
                    {
                        lineBuffer.push_back(' ');
                        ++column;   

                    }

                } else if ((c & 0x80) == 0)
                {
                    lineBuffer.push_back(c);
                    ++column;
                } else {
                    lineBuffer.push_back(c);
                    if ((c & 0xC0) == 0x80)
                    {
                        ++column;
                    }
                }
            }
            if (column >= indent)
            {
                WriteLine();
            }
        }
        void Write(const std::string &text)
        {
            for (char c : text)
            {
                if (c == '\n')
                {
                    WriteLine();
                }
                else
                {
                    if (column <= indent)
                    {
                        if (c == ' ')
                        {
                            continue;
                        }
                        while (column < indent)
                        {
                            lineBuffer.push_back(' ');
                            ++column;
                        }
                    }
                    if (c == '\t')
                    {
                        lineBuffer.push_back(' ');
                        ++column;
                        while ((column % tabSize) != 0)
                        {
                            lineBuffer.push_back(' ');
                            ++column;
                        }
                    }
                    else if ((c & 0x80) == 0)
                    {
                        lineBuffer.push_back(c);
                        ++column;
                    }
                    else
                    {
                        //utf-8 sequence.
                        lineBuffer.push_back(c);
                        if ((c & 0xC0) == 0x80)
                        {
                            ++column;
                        }
                    }

                    if (column >= lineWidth - 1)
                    {

                        size_t breakPos = FindBreak();

                        size_t startOfOverflow = breakPos;
                        while (startOfOverflow < lineBuffer.size() && lineBuffer[startOfOverflow] == ' ')
                        {
                            ++startOfOverflow;
                        }
                        std::string overflow = string(&lineBuffer[startOfOverflow], lineBuffer.size() - startOfOverflow);
                        lineBuffer.resize(breakPos);

                        WriteLine();

                        this->Write(overflow);
                    }
                }
            }
        }

    private:
        size_t FindBreak()
        {
            size_t i = lineBuffer.size();
            while (i > indent && lineBuffer[i-1] != ' ')
            {
                --i;
            }
            if (i > indent)
                return i;
            return lineBuffer.size();
        }
    };


    template <typename T> 
    PrettyPrinter& operator<< (PrettyPrinter &p, T &v)
    {
        std::stringstream s;
        s << v;
        p.Write(s.str());
        return p;
    }

    inline PrettyPrinter& operator << (PrettyPrinter& p,const std::string&s)
    {
        p.Write(s);
        return p;
    }

}