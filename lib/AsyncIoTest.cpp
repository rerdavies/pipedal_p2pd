#include "p2psession/CoTask.h"
#include "p2psession/AsyncIo.h"
#include <cassert>
#include <chrono>
#include "p2psession/AsyncFile.h"
#include "ss.h"

using namespace p2psession;
using namespace std;


//////////////// Read Fast Write Slow  ////////////////////////

CoTask<> CoSlowWriter(std::unique_ptr<AsyncFile> writer, bool trace)
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
    writer->Close();
    co_return;
}

constexpr size_t FAST_WRITE_SIZE = 1024*1024;

CoTask<> CoFastWriter(std::unique_ptr<AsyncFile> writer, size_t lengthBytes,bool trace)
{
    int i = 0;
    std::string msg;

    char buffer[1024];

    while (lengthBytes != 0)
    {
        for (int j = 0; j < sizeof(buffer); ++j)
        {
            buffer[j] =  'a' + (i % 26);
        }
        size_t thisTime = lengthBytes;
        if (thisTime > sizeof(buffer)) thisTime = sizeof(buffer);

        co_await writer->CoWrite(buffer, thisTime);
        lengthBytes -= thisTime;
    }
    writer->Close();
    co_return;
}

CoTask<> CoSlowReader(std::unique_ptr<AsyncFile> reader, size_t expectedBytes,bool trace)
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
        int offset = 0;
        while (offset < sizeof(buffer))
        {
            lastRead = co_await reader->CoRead(buffer+offset,sizeof(buffer)-offset);
            totalRead += lastRead;
            offset += lastRead;
            if (lastRead == 0) break;
        }
        if (lastRead == 0) break;
        ++nBuffer;
    }
    reader->Close();
    cout << endl;
    cout << "        Total read: " << totalRead << endl;
    if (totalRead != expectedBytes)
    {
        std::string msg = SS("Incorrect number of bytes read. Expecting " << expectedBytes << " bytes.");
        throw std::logic_error(msg.c_str());
    }
}

CoTask<> CoFastReader(std::unique_ptr<AsyncFile> reader,bool trace)
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
    reader->Close();
    co_return;
}

/////////////////////////

CoTask<> CoWriteFastReadSlow()
{
    cout << "    Read Slow Write Fast" << endl;

    try
    {
        constexpr size_t TEST_BYTES = 10*1024*1024;
        std::unique_ptr<AsyncFile> reader;
        std::unique_ptr<AsyncFile> writer;
        AsyncFile::CreateSocketPair(&reader, &writer);

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
        std::unique_ptr<AsyncFile> reader;
        std::unique_ptr<AsyncFile> writer;
        AsyncFile::CreateSocketPair(&reader, &writer);

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