#pragma once
#include "cotask/CoExec.h"
#include <string>
#include "cotask/Log.h"
namespace p2p
{
    using namespace cotask;

    class DnsMasqProcess
    {
    public:
        ~DnsMasqProcess() { Stop().GetResult(); }
        
        void Start(std::shared_ptr<ILog> log, const std::string &interfaceName);

        CoTask<> Stop();

        bool HasTerminated() const { return process.HasTerminated(); }
    private:
        bool running = false;
        int terminatedThreads = 0;
        CoConditionVariable cv;
        CoTask<> CopyStdoutToDebugLog();
        CoTask<> CopyStderrToErrorLog();
        std::shared_ptr<ILog> logLifetime;
        ILog *log;
        CoExec process;
    };
}