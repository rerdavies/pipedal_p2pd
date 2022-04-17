#include "includes/DnsMasqProcess.h"
#include "includes/P2pConfiguration.h"
#include <vector>

using namespace p2p;
using namespace cotask;
using namespace std;

void DnsMasqProcess::Start(std::shared_ptr<ILog> log, const std::string &interfaceName)
{
    this->terminatedThreads = 0;
    logLifetime = log;
    this->log = log.get();

    auto &config = gP2pConfiguration;

    if (std::filesystem::exists(config.dhcpLeaseFilePath))
    {
        std::filesystem::path t { config.dhcpLeaseFilePath};
        if (!t.has_relative_path())
        {
            throw std::invalid_argument("Invalid config.dhcpLeaseFilePath");
        }
        auto parent = t.parent_path();
        
        std::filesystem::create_directories(t.parent_path());
    }

    std::vector<std::string> args{
        "dnsmasq",
        "--keep-in-foreground",
        "--interface=" + interfaceName,
        "--no-resolv",
        "--dhcp-range=172.24.0.3,172.24.10.127,1h",
        "--domain=local",
        "--address=/pipedal.local/172.24.0.1/",
        "--dhcp-leasefile=" + config.dhcpLeaseFilePath, // since we must NOT use the system conf files.
        "--conf-file=" + config.dhcpConfFile,
    };

    process.Execute("sudo", args);
    running = true;

    Dispatcher().StartThread(CopyStderrToErrorLog());
    Dispatcher().StartThread(CopyStdoutToDebugLog());
}

CoTask<> DnsMasqProcess::Stop()
{
    if (running)
    {
        running = false;
        bool terminated = false;
        process.Kill(CoExec::SignalType::Terminate);
        try
        {
            co_await cv.Wait(
                5000ms,
                [this]() {
                    return terminatedThreads == 2; // wait for stdout and stderr to close.
                });
            terminated = true;
        }
        catch (const CoTimedOutException & /*ignored*/)
        {
        }
        if (!terminated)
        {
            process.Kill(CoExec::SignalType::Kill);
            co_await cv.Wait(
                [this]() {
                    return terminatedThreads == 2; // wait for stdout and stderr to close.
                });
        }
        process.Wait(); // non-blocking after standard output has completed.
    }
}

CoTask<> DnsMasqProcess::CopyStdoutToDebugLog()
{
    std::string line;

    while (co_await process.Stdout().CoReadLine(&line))
    {
        if (line.length() != 0)
        {
            log->Debug("dnsmasq: " + line);
        }
    }
    cv.Notify([this]() {
        this->terminatedThreads++;
    });
}
CoTask<> DnsMasqProcess::CopyStderrToErrorLog()
{
    std::string line;

    while (co_await process.Stderr().CoReadLine(&line))
    {
        if (line.length() != 0)
        {
            log->Error("dnsmasq: " + line);
        }
    }
    cv.Notify([this]() {
        this->terminatedThreads++;
    });
}
