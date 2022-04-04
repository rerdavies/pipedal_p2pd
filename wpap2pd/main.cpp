
#include "cotask/CoTask.h"
#include "includes/P2pSessionManager.h"
#include "CommandLineParser.h"
#include <exception>
#include <iostream>
#include <string>
#include <filesystem>

using namespace p2p;
using namespace cotask;
using namespace std;
using namespace twoplay;

CoTask<> CoMain(int argc, const char *const *argv)
{

    string interfaceOption = "p2p-dev-wlan0";
    try
    {
        CommandLineParser parser;
        parser.AddOption("i", &interfaceOption);
        parser.Parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
    }
    try
    {
        P2pSessionManager sessionManager;
        sessionManager.Log().SetLogLevel(LogLevel::Debug);

        co_await sessionManager.Open(argc, argv);

        sessionManager.Scan();
       
        while (true)
        {
            co_await CoDelay(1000ms);
        }
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
    }
    co_return;
}

int main(int argc, const char **argv)
{
    Dispatcher().MessageLoop(CoMain(argc, argv));
    return 0;
}