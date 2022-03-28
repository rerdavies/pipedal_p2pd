
#include "p2psession/CoTask.h"
#include "P2pSessionManager.h"
#include "CommandLineParser.h"
#include <exception>
#include <iostream>
#include <string>
#include <filesystem>

using namespace twoplay;
using namespace std;

int main(int argc, const char **argv)
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
    {
        try
        {
            P2pSessionManager sessionManager;
            sessionManager.SetLogLevel(LogLevel::Debug);

            std::filesystem::path path = std::filesystem::path("/var/run/wpa_supplicant") / interfaceOption;

            sessionManager.Open(path);

            sessionManager.Run();
            sessionManager.Close();

        }
        catch (const std::exception &e)
        {
            cout << "Error: " << e.what() << endl;
        }
    }
}