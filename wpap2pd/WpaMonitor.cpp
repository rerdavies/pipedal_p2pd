#include "includes/WpaCliWrapper.h"
#include <cotask/Os.h>
#include <cotask/CoEvent.h>

using namespace cotask;
using namespace std;


void WpaCliWrapper::Execute(const std::vector<std::string> &arguments)
{
    std::filesystem::path path = os::FindOnPath("wpa_cli");
    base::Execute(path,arguments);
}
void WpaCliWrapper::Execute(const std::string &deviceName)
{
    vector<string> args;
    args.push_back("-i");
    args.push_back(deviceName);
}



CoTask<> WpaCliWrapper::ProcessMessages()
{
    Dispatcher().StartThread(ReadMessages());
    co_await WriteMessages();
}

CoTask<> WpaCliWrapper::WriteMessages()
{
    co_await Stdin().CoWriteLine("scan");
    co_return;
}
CoTask<> WpaCliWrapper::ReadMessages() {
    WpaEvent event;
    std::string line;
    try {
    while (true)
    {

        bool result =  co_await Stdout().CoReadLine(&line);
        if (!result) break; // eof.

        if (traceMessages)
        {
            cout << ": " << line << endl;
        }
        if (event.ParseLine(line.c_str()))
        {
            OnEvent(event);
        }
    }
    } catch (const std::exception&e)
    {

    }
    cout << "Process terminated." << endl;
}
CoTask<> WpaCliWrapper::ReadErrors() {
    WpaEvent event;
    std::string line;
    try {
    while (true)
    {

        bool result =  co_await Stderr().CoReadLine(&line);
        if (!result) break; // eof.

        if (traceMessages)
        {
            cout << "* " << line << endl;
        }
        if (event.ParseLine(line.c_str()))
        {
            OnEvent(event);
        }
    }
    } catch (const std::exception&e)
    {

    }
}


CoTask<> WpaCliWrapper::Run(int argc, const char *const*argv)
{
    vector<string> args;

    for (int i = 0; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--") 
        {
            args.resize(0);
        } else if (arg == "--trace")
        {
            this->traceMessages = true;
        } else  {
            args.push_back(argv[i]);
        }
    }
    Execute(args);

    co_await CoOnInit();

    co_await ProcessMessages();


    co_return;
}

CoTask<std::vector<std::string>> WpaCliWrapper::ReadToPrompt(std::chrono::milliseconds timeout)
{
    std::vector<std::string> response;
    while (true)
    {
        while (lineBufferHead != lineBufferTail)
        {
            char c = lineBuffer[lineBufferHead];
            if (c == '\n')  {
                response.push_back(lineResult.str());
                lineResult.clear();
                lineResultEmpty = true;
            } else if (lineResultEmpty && c == '>') {
                if (lineBufferRecoveringFromTimeout)
                {
                    lineBufferRecoveringFromTimeout = true;
                    response.resize(0);
                } else {
                    NotifyPrompting();
                    co_return response;
                }
            } else {
                lineResultEmpty = false;
                lineResult << c;
            }
        }
        try {
            int nRead = co_await Stdin().CoRead(lineBuffer,sizeof(lineBuffer),timeout);
            lineBufferHead = 0; 
            lineBufferTail = nRead;
        } catch (const CoTimedOutException &e)
        {
            lineBufferRecoveringFromTimeout = true;
            throw;
        }
    }
    co_return response;
}


