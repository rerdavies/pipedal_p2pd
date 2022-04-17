#include "cotask/CoExec.h"
#include "cotask/Os.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

using namespace cotask;
using namespace cotask::os;

static void PrepareFile(int file)
{
    // remove CLOEXEC.
    int fdflags = fcntl(file, F_GETFD);
    fdflags &= ~FD_CLOEXEC;
    fcntl(file, F_SETFD, fdflags);

    // remove NONBLOCKING.
    int statusFlags = fcntl(file, F_GETFL);
    statusFlags &= ~O_NONBLOCK;
    fcntl(file, F_SETFL, statusFlags);
}

// see man 7 environ! This is legitimate.
extern char **environ;

void CoExec::Execute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments)
{
    std::vector<std::string> environment;
    for (char **p = environ; *p != nullptr; ++p)
    {
        environment.push_back(*p);
    }
    Execute(pathName, arguments, environment);
}

void CoExec::Execute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments,
    const std::vector<std::string> &environment

)
{
    auto fullPath = FindOnPath(pathName);
    CoFile remoteStdin, remoteStdout, remoteStderr;

    CoFile::CreateSocketPair(this->stdin, remoteStdin);
    CoFile::CreateSocketPair(this->stdout, remoteStdout);
    CoFile::CreateSocketPair(this->stderr, remoteStderr);

    int rstdIn = remoteStdin.Detach();
    PrepareFile(rstdIn);
    int rstdOut = remoteStdout.Detach();
    PrepareFile(rstdOut);
    int rstdErr = remoteStderr.Detach();
    PrepareFile(rstdErr);

    this->processId = os::Spawn(fullPath, arguments, environment, rstdIn, rstdOut, rstdErr);
}
CoExec::~CoExec()
{
    CoKill().GetResult();
}

CoTask<> CoExec::CoKill(std::chrono::milliseconds gracePeriod)
{
    Kill(SignalType::Terminate);
    try {
        co_await this->CoWait(gracePeriod);
        co_return;
    } catch (CoTimedOutException &e)
    
    {

    }
    Kill(SignalType::Kill);
    co_await this->CoWait();
    co_return;
}
CoTask<bool> CoExec::CoWait(std::chrono::milliseconds timeout)
{
    std::chrono::milliseconds maxTime = Dispatcher().Now() + timeout;
    while (!HasTerminated())
    {
        co_await CoDelay(100ms);
        if (Dispatcher().Now() > maxTime)
        {
            throw CoTimedOutException();
        }
    }
    co_return Wait();
}

bool CoExec::HasTerminated() const {
    if (this->processId == ProcessId::Invalid) return true;
    int status = 0;
    // posix/ ISO C00.
    int rc = waitpid((pid_t)processId,&status,WNOHANG);
    if (rc == 0) return false;
    if (rc < 0) {
        CoIoException::ThrowErrno();
    }
    if (WIFEXITED(status)) {
        return true;
    }
    if (WIFSIGNALED(status))
    {
        return true;
    }
    return false;
}

bool CoExec::Wait(std::chrono::milliseconds timeoutMs)
{
    if (this->processId != os::ProcessId::Invalid)
    {
        exitResult = os::WaitForProcess(processId, timeoutMs.count());
        this->processId = os::ProcessId::Invalid;
    }
    return exitResult;
}

CoTask<> CoExec::OutputReader(CoFile &file, std::ostream &outputStream)
{

    std::string line;

    cvOutput.Execute([this] {
        ++activeOutputs;
    });
    while (co_await file.CoReadLine(&line))
    {
        cvOutput.Execute([&line, &outputStream]() {
            outputStream << line << std::endl;
        });
    }
    cvOutput.Notify([this]() {
        --activeOutputs;
    });
    co_return;
}

CoTask<bool> CoExec::CoExecute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments,
    std::string *output)
{
    CoConditionVariable cvOutput;
    std::stringstream outputStream;

    this->Execute(pathName, arguments);

    Dispatcher().StartThread(OutputReader(this->Stdout(), outputStream));
    Dispatcher().StartThread(OutputReader(this->Stderr(), outputStream));

    if (output != nullptr)
    {
        *output = outputStream.str();
    }
    co_await cvOutput.Wait([this] { return this->activeOutputs == 0; });
    co_return co_await CoWait(); // to clear the zombie task.
}

CoTask<> CoExec::DiscardingOutputReader(CoFile &file)
{
    cvOutput.Execute([this] { ++activeOutputs; });

    try
    {
        char buffer[512];
        while (true)
        {
            int nRead = co_await file.CoRead(buffer, sizeof(buffer));
            if (nRead == 0)
                break;
        }
    }
    catch (const std::exception & /*ignored*/)
    {
    }
    cvOutput.Notify([this] { --activeOutputs; });

    co_return;
}
void CoExec::DiscardOutput(CoFile &file)
{
    if (&file != &Stdout() && &file != &Stderr())
    {
        throw std::invalid_argument("Must be either Stdout() or Stderr()");
    }
    Dispatcher().StartThread(DiscardingOutputReader(file));
}

void CoExec::Kill(SignalType signalType)
{
    if (processId == ProcessId::Invalid) return;
    // posix/iso c99.
    switch (signalType)
    {
        case SignalType::Interrupt:
            kill((pid_t)processId,SIGINT);
            break;
        case SignalType::Terminate:
            kill((pid_t)processId,SIGTERM);
            break;
        case SignalType::Kill:
            kill((pid_t)processId,SIGKILL);
            break;
    }
}
