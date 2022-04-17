
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
#include "ss.h"
#include "includes/PrettyPrinter.h"
#include "includes/P2pConfiguration.h"
#if __linux__
#include <systemd/sd-daemon.h>
#endif

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

static void printHelp()
{
    PrettyPrinter p;

    p << "wpap2pd v1.0\n"
      << "Copyright 2022 Robin E. R. Davies.\n"
      << "\n"
      << "Usage:\n";
    p << "\twpap2pd [options]*\n\n";
    p << "Options:\n";

    p.Indent(20);
    p.Hang(" -?, --help");
    p << "Help. Print this message.\n\n";

    p.Hang(" -c, --config-file=FILE");
    p << "Use this configuration file.\n\n";

#ifdef __linux__
    p.Hang(" -D, --systemd");
    p << "Run under systemd. (Uses systemd logging instead of console logging.\n\n";
#endif

    p.Hang(" -i, ---wlan-interface=FILE");
    p << "wlan interface (default wlan0)\n\n";

    p.Hang(" --log-level=debug|info|warning|error");
    p << "Set log level (default info)\n\n";

    p.Hang(" --trace-message");
    p << "Log all communication with wpa_supplication at info log-level \n\n";

    p.Indent(4);
    p.Hang("Remarks:");

    p << "wpap2pd provides session management for Wifi p2p (Wifi Direct) connections when "
         "using wpa_supplicant.\n\n";

    p.Indent(4);
    p.Hang("Example:");

    p << "wpap2pd -c /etc/wpap2pd.conf -i wlan1\n\n";

    p.Indent(4);
    p.Hang("Configuration file format:");

    p << "Key-value pairs separated by '='. String values "
         "may optionally be surrounded by double quotes. "
         "Within quoted strings, only the following escape values "
         "are supported: \\n \\r \\\\ \\\". "
         "The '#' character can be used to mark comments. All content "
         "after the '#', to the following end-of-line is discarded."
         "\n\n";
    p.Indent(8);
    p.Hang("\tConfiguration file values:\n\n");

    p.Hang("\tcountry_code=XX");
    p << "WiFi regdomain iso-3166 2-letter country code (with some extensions).\n\n"
         "(see: http://www.davros.org/misc/iso3166.txt)\n\n";

    p.Hang("\tp2p_pin=12345678");
    p << "8-digit pin for use during P2P label authentication.\n\n";

    p.Hang("\tp2p_devicename=\"string\"");
    p << "Display name when connection via p2p. Not to exceed 31 UTF-8 octets.\n\n";

    p.Hang("\tp2p_ssid_postfix=\"string\"");
    p << "Postfix of the groupname as it appears (for example) in the Android WiFi Direct group list. "
         "Appended to \"DIRECT-XX-\". Not to exceed 21 UTF-8 octets.\n\n";

    p.Hang("\twifiGroupFrequency=integer");
    p << "The WiFi channel frequency to use for group communications, in kHz. \n\n";
    p << "For optimal search times, should almost always be a social channel: 2412 (ch1),2437 (ch6), or 2462 (ch11).\n\n";

    p.Hang("\t"
           "p2p_model_name=\"string\"");
    p << "Model name (used in P2P connection negotiations). Can be empty.\n\n";

    p.Hang("\t"
           "p2p_model_number=\"string\"");
    p << "Model number (used in P2P connection negotiations). Can be empty.\n\n";

    p.Hang("\t"
           "p2p_manufacturer=\"string\"");
    p << "Manufacturer name (used in P2P connection negotiations). Can be empty.\n\n";

    p.Hang("\t"
           "p2p_serial_number=\"string\"");
    p << "Device serial number (used in P2P connection negotiations). Can be empty.\n\n";

    p.Hang("\t"
           "p2p_device_type=string");
    p << "P2p device type (used in P2P connection negotiations). Must not be empty. e.g.: 1-0050F204-1\n\n";

    p.Hang("\t"
           "wlanInterface=string");
    p << "Name of the wlan device interface. (Default=wlan0)\n\n";

    p.Hang("\t"
           "p2pInterface=string");
    p << "Name of the p2p device interface. (Default=p2p-dev-wlan0)\n\n";

    p.Hang("\t"
           "p2p_go_ht40=true|false");
    p << "Whether to enable ht40 Wifi connections on the group WiFichannel.\n\n";

    p.Hang("\t"
           "p2p_go_vht=true|false");
    p << "Whether to enable vht Wifi connections on the group WiFichannel.\n\n";

    p.Hang("\t"
           "p2p_go_he=true|false");
    p << "Whether to enable vht Wifi connections on the group WiFichannel.\n\n";

    p.Hang("\t"
           "p2p_ip_address=n.n.n.n/nn");
    p << "IPv4 address to use on the group channel. e.g. 172.24.0.2/16\n\n";

    p.Hang("\t"
           "service_guid_file=FILENAME");
    p << "Name of a file containing the device-specific GUID used to indentify the current device. File syntax: Syntax: 0a6045b0-1753-4104-b3e4-b9713b9cc356\\n\n\n";
    p << "Usually, this will match an avahi DNS-SD service TXT record used to find services on the device once connected. "
         "wpap2pd does not publish a DNS-SD service record.\n\n";

    p.Hang("\t"
           "service_guid=UUID");
    p << "Device identifier to use if service_guid_file was not specified. Syntax: 0a6045b0-1753-4104-b3e4-b9713b9cc356\n\n";
}

static bool  LoadConfig(const std::string&configFile)
{
    auto& config = gP2pConfiguration;

    try {
        config.Load(configFile);
    } catch (const std::exception & e)
    {
        cout << "Error: Failed to load config file. " << e.what() << endl;
        return false;
    }
    return true;
}
static void PrintConfig() {
    auto& config = gP2pConfiguration;
    config.Save(cout);
}

CoTask<int> CoMain(int argc, const char *const *argv)
{

    string interfaceOption = "wlan0";
    string logLevel = "info";
    std::string configFile;
    bool help = false;
    bool systemd = false;
    bool print_config = false;

    bool traceMessages = false;

    bool parsed = false;
    try
    {
        CommandLineParser parser;
        parser.AddOption("-?", &help);
        parser.AddOption("--help", &help);
        parser.AddOption("-i", &interfaceOption);
        parser.AddOption("-wlan-interface", &interfaceOption);
        parser.AddOption("-c", &configFile);
        parser.AddOption("--config-file", &configFile);
        parser.AddOption("--log-level", &logLevel);
        parser.AddOption("--trace-messages", &traceMessages);
        parser.AddOption("-D", &systemd);
        parser.AddOption("--systemd", &systemd);
        parser.AddOption("--print-config", &print_config); // debug artifact

        parser.Parse(argc, argv);
        parsed = true;
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        Dispatcher().PostQuit();
    }
    if (!parsed)
    {
        co_return EXIT_FAILURE; // vs-code analysis doesn't like co_return in a catch block. (not actualy correct. but this makes the warning go away.)
    }
    if (help)
    {
        printHelp();
        co_return EXIT_SUCCESS;
    }

    if (configFile != "")
    {
        if (!LoadConfig(configFile))
        {
            co_return EXIT_FAILURE;
        }
    }
    if (print_config)
    {
        PrintConfig();
        co_return EXIT_SUCCESS;
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
    else
    {
        throw invalid_argument("Invalid -log option. Expection debug, info, warning or error.");
    }

    try
    {
        AsyncIo::GetInstance().Start();

        P2pSessionManager sessionManager;
        sessionManager.SetLog(log);
        sessionManager.TraceMessages(traceMessages);
        co_await sessionManager.Open(interfaceOption);

#ifdef __linux__
        if (systemd)
        {
            sd_notifyf(0, "READY=1\n"
                        "MAINPID=%lu",
                        (unsigned long)getpid());
        }
#endif
        while (shutdown_flag && !sessionManager.IsFinished())
        {
            co_await CoDelay(300ms);
        }

#ifdef __linux__
        if (systemd)
        {
            sd_notify(0, "STOPPING=1");
        }
#endif

        log->Info("Shutting down...");
        // orderly shutdown.
        co_await sessionManager.Close();
    }
    catch (const std::exception &e)
    {
#ifdef __linux__
    if (systemd)
    {
        sd_notifyf(0,"STATUS=Unexpected error: %s",e.what());
    }
#endif
        log->Error(SS("Terminating abnormally. " << e.what()));
    }
    log->Info("Shutdown complete.");
    Dispatcher().PostQuit();

    co_return EXIT_FAILURE;
}

int main(int argc, const char **argv)
{

    // signals have to be established before CoDispatcher thread pool is established!
    signal(SIGTERM, onSigInt);
    signal(SIGINT, onSigInt);

    int retVal = CoMain(argc, argv).GetResult();

    AsyncIo::GetInstance().Stop();

    return retVal;
}