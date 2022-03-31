#include "p2psession/WpaCliWrapper.h"
#include "p2psession/Os.h"
#include "p2psession/CoEvent.h"

using namespace p2psession;
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



Task<> WpaCliWrapper::ProcessMessages()
{
    WpaEvent event;
    std::string line;
    while (true)
    {

        bool result =  co_await Stdin().CoReadLine(&line);
        if (!result) break; // eof.

        if (traceMessages)
        {
            cout << line << endl;
        }
        if (event.ParseLine(line.c_str()))
        {
            OnEvent(event);
        }
    }
}

Task<> WpaCliWrapper::Run(int argc, char **argv)
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
