
#include "cotask/CoTask.h"
#include "includes/P2pSessionManager.h"
#include "CommandLineParser.h"
#include <exception>
#include <iostream>
#include <string>
#include <filesystem>
#include <signal.h>
#include <unistd.h>
#include <memory.h>

using namespace p2p;
using namespace cotask;
using namespace std;
using namespace twoplay;

volatile sig_atomic_t shutdown_flag = 1;
volatile sig_atomic_t signal_abort = 1;

void onSigInt(int signal)
{
    if (signal_abort)
    {
        exit(EXIT_FAILURE);
    }
    shutdown_flag = 0;
}

#define errExit(msg)        \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

CoTask<> CoMain(int argc, const char *const *argv)
{

    string interfaceOption = "wlan0";
    string logLevel = "info";
    bool traceMessages = false;
    try
    {
        CommandLineParser parser;
        parser.AddOption("-i", &interfaceOption);
        parser.AddOption("--log", &logLevel);
        parser.AddOption("--trace-messages", &traceMessages);
        parser.Parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        Dispatcher().PostQuit();
        co_return;
    }
    signal_abort = 0; // signals cancel instead of aborting.

    std::shared_ptr<ILog> log = std::make_shared<ConsoleLog>();

    if (logLevel == "debug")
    {
        log->SetLogLevel(LogLevel::Debug);
    }
    else if (logLevel == "warning")
    {
        log->SetLogLevel(LogLevel::Warning);
    }
    else if (logLevel == "error")
    {
        log->SetLogLevel(LogLevel::Error);
    }
    else if (logLevel == "info")
    {
        log->SetLogLevel(LogLevel::Info);
    }
    else {
        throw invalid_argument("Invalid -log option. Expection debug, info, warning or error.");
    }

    try
    {

        P2pSessionManager sessionManager;
        sessionManager.SetLog(log);
        sessionManager.TraceMessages(traceMessages);
        co_await sessionManager.Open(interfaceOption);

        sessionManager.Log().Info("Started.");
        while (shutdown_flag  && !sessionManager.IsComplete())
        {
            co_await CoDelay(300ms);
        }
        log->Info("Shutting down...");
        // orderly shutdown.
        co_await sessionManager.Close();
    }
    catch (const std::exception &e)
    {
        log->Error(e.what());
    }
    log->Info("Shutdown complete.");
    Dispatcher().PostQuit();
    co_return;
}

int main(int argc, const char **argv)
{

    // signals have to be established before CoDispatcher thread pool is established!
    signal(SIGTERM, onSigInt);
    signal(SIGINT, onSigInt);

    Dispatcher().MessageLoop(CoMain(argc, argv));
    return 0;
}