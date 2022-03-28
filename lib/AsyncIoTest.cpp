#include "p2psession/CoTask.h"
#include "p2psession/AsyncIo.h"
#include <cassert>
#include <chrono>
#include "p2psession/AsyncFile.h"
#include "ss.h"

using namespace p2psession;
using namespace std;

std::filesystem::path GetTempFile()
{
    filesystem::path path = tmpnam(nullptr);
    return path;
}
//////////////// Read Fast Write Slow  ////////////////////////

Task<> CoSlowWriter(std::unique_ptr<AsyncFile> writer, bool trace)
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

Task<> CoFastWriter(std::unique_ptr<AsyncFile> writer, size_t lengthBytes,bool trace)
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

        for (ssize_t offset = 0; offset < thisTime; /**/)
        {
            ssize_t nWritten = co_await writer->CoWrite(buffer+offset,sizeof(buffer)-offset);
            lengthBytes -= nWritten;
            offset += nWritten;

        }
    }
    writer->Close();
    co_return;
}

Task<> CoSlowReader(std::unique_ptr<AsyncFile> reader, size_t expectedBytes,bool trace)
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

Task<> CoFastReader(std::unique_ptr<AsyncFile> reader,bool trace)
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

Task<> CoWriteFastReadSlow()
{
    cout << "    Read Slow Write Fast" << endl;

    try
    {
        constexpr size_t TEST_BYTES = 10*1024*1024;
        std::unique_ptr<AsyncFile> reader;
        std::unique_ptr<AsyncFile> writer;
        AsyncFile::CreateSocketPair(&reader, &writer);

        std::unique_ptr<Task<>> task = 
           std::make_unique<Task<>>(CoFastWriter(std::move(writer),TEST_BYTES,false));
        CoDispatcher::CurrentDispatcher().StartThread(std::move(task));

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
Task<> CoReadFastWriteSlowTest()
{
    cout << "Read Fast Write Slow" << endl;

    try
    {
        std::unique_ptr<AsyncFile> reader;
        std::unique_ptr<AsyncFile> writer;
        AsyncFile::CreateSocketPair(&reader, &writer);

        std::unique_ptr<Task<>> task = std::make_unique<Task<>>(CoSlowWriter(std::move(writer),true));
        CoDispatcher::CurrentDispatcher().StartThread(std::move(task));

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

    Task<> task = CoWriteFastReadSlow();
    task.GetResult();

    task = CoReadFastWriteSlowTest();
    task.GetResult();


    cout << "Done." << endl;
}

///////////////////////////////////////////////////////////

int main(int argc, char **argv)
{

    ReadWriteTest();
    return 0;
}