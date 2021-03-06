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

#include "cotask/CoTask.h"
#include "cotask/AsyncIo.h"
#include <cassert>
#include <chrono>
#include "cotask/CoFile.h"
#include "ss.h"

using namespace cotask;
using namespace std;


//////////////// Read Fast Write Slow  ////////////////////////

CoTask<> CoSlowWriter(std::unique_ptr<CoFile> writer, bool trace)
{
    int i = 0;
    std::string msg;

    msg = SS("Message " << (i++) << endl);
    if (trace) cout << "    >" << msg;
    co_await writer->CoWrite(msg.c_str(), msg.length());
    co_await CoDelay(1000ms);

    msg = SS("Message " << (i++) << endl);
    if (trace) cout << "    >" << msg;
    co_await writer->CoWrite(msg.c_str(), msg.length());
    co_await CoDelay(1000ms);

    for (int i = 2; i < 5; ++i)
    {
        msg = SS("Message " << i << endl);
        if (trace) cout << "    >" << msg;
        co_await writer->CoWrite(msg.c_str(), msg.length());
        co_await CoDelay(500ms);
    }
    co_await writer->CoClose();
    co_return;
}

constexpr size_t FAST_WRITE_SIZE = 1024*1024;

CoTask<> CoFastWriter(std::unique_ptr<CoFile> writer, size_t lengthBytes,bool trace)
{
    int i = 0;
    std::string msg;

    char buffer[1024];

    while (lengthBytes != 0)
    {
        for (size_t j = 0; j < sizeof(buffer); ++j)
        {
            buffer[j] =  'a' + (i % 26);
        }
        size_t thisTime = lengthBytes;
        if (thisTime > sizeof(buffer)) thisTime = sizeof(buffer);

        co_await writer->CoWrite(buffer, thisTime);
        lengthBytes -= thisTime;
    }
    co_await writer->CoClose();
    co_return;
}

CoTask<> CoSlowReader(std::unique_ptr<CoFile> reader, size_t expectedBytes,bool trace)
{
    char buffer[113];


    int nReads = expectedBytes/sizeof(buffer);

    chrono::milliseconds testTime {20000};
    chrono::milliseconds bufferWaitTime {testTime.count()/nReads};

    int nBuffer = 0;
    int currentDot = 0;

    size_t totalRead = 0;

    cout << "    ";
    while (true)
    {
        int dots = totalRead*60LL/expectedBytes;
        for (/* currentDot*/;  currentDot < dots;++currentDot)
        {
            cout << '.';
            cout.flush();
        }

        int lastRead = 0;
        size_t offset = 0;
        while (offset < sizeof(buffer))
        {
            lastRead = co_await reader->CoRead(buffer+offset,sizeof(buffer)-offset);
            totalRead += lastRead;
            offset += lastRead;
            if (lastRead == 0) break;
        }
        if (lastRead == 0) 
            break;
        ++nBuffer;
    }
    co_await reader->CoClose();
    cout << endl;
    cout << "        Total read: " << totalRead << endl;
    if (totalRead != expectedBytes)
    {
        std::string msg = SS("Incorrect number of bytes read. Expecting " << expectedBytes << " bytes.");
        throw std::logic_error(msg.c_str());
    }
}

CoTask<> CoFastReader(std::unique_ptr<CoFile> reader,bool trace)
{
    char buffer[1024];
    while (true)
    {
        int nRead = co_await reader->CoRead(buffer, sizeof(buffer) - 1);
        if (nRead == 0)
            break;
        buffer[nRead] = 0;
        if (trace)
        {
            cout << "      <" << buffer;
            cout.flush();
        }
    }
    co_await reader->CoClose();
    co_return;
}

/////////////////////////

CoTask<> CoWriteFastReadSlow()
{
    cout << "    Read Slow Write Fast" << endl;

    try
    {
        constexpr size_t TEST_BYTES = 10*1024*1024;
        std::unique_ptr<CoFile> reader;
        std::unique_ptr<CoFile> writer;
        CoFile::CreateSocketPair(&reader, &writer);

        CoDispatcher::CurrentDispatcher().StartThread(
            CoFastWriter(std::move(writer),TEST_BYTES,false)
        );

        co_await CoSlowReader(std::move(reader),TEST_BYTES,false);
    }
    catch (const std::exception &e)
    {
        cout << "Error: Unexpected exception: " << e.what() << endl;
        throw;
    }
    co_return;
}

///////////////////////////
CoTask<> CoReadFastWriteSlowTest()
{
    cout << "Read Fast Write Slow" << endl;

    try
    {
        std::unique_ptr<CoFile> reader;
        std::unique_ptr<CoFile> writer;
        CoFile::CreateSocketPair(&reader, &writer);

        CoDispatcher::CurrentDispatcher().StartThread(CoSlowWriter(std::move(writer),true));

        co_await CoFastReader(std::move(reader),true);
    }
    catch (const std::exception &e)
    {
        cout << "Error: Unexpected exception: " << e.what() << endl;
        throw;
    }
    co_return;
}

void ReadWriteTest()
{
    cout << "--- ReadWriteTest ---" << endl;

    CoTask<> task = CoWriteFastReadSlow();
    task.GetResult();

    task = CoReadFastWriteSlowTest();
    task.GetResult();


    cout << "Done." << endl;
}

///////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    ReadWriteTest();

    Dispatcher().DestroyDispatcher();
    return 0;
}