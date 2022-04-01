#include "cotask/CoExec.h"
#include "cotask/Os.h"
#include <fcntl.h>

using namespace cotask;
using namespace cotask::os;

static void PrepareFile(int file)
{
    // remove CLOEXEC.
    int fdflags = fcntl(file,F_GETFD);
    fdflags &= ~FD_CLOEXEC;
    fcntl(file,F_SETFD,fdflags);

    // remove NONBLOCKING.
    int statusFlags = fcntl(file,F_GETFL);
    statusFlags &= ~O_NONBLOCK;
    fcntl(file,F_SETFL,statusFlags);

}


// see man 7 environ! This is legitimate.
extern char **environ;

void CoExec::Execute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments
)
{
    std::vector<std::string> environment;
    for (char**p = environ; *p != nullptr; ++p)
    {
        environment.push_back(*p);
    }
    Execute(pathName,arguments,environment);
}

void CoExec::Execute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments,
    const std::vector<std::string> &environment

    )
{
    auto fullPath = FindOnPath(pathName);
    CoFile remoteStdin,remoteStdout,remoteStderr;

    CoFile::CreateSocketPair(this->stdin,remoteStdin);
    CoFile::CreateSocketPair(this->stdout,remoteStdout);
    CoFile::CreateSocketPair(this->stderr,remoteStderr);

    int rstdIn = remoteStdin.Detach();
    PrepareFile(rstdIn);
    int rstdOut = remoteStdout.Detach();
    PrepareFile(rstdOut);
    int rstdErr = remoteStderr.Detach();
    PrepareFile(rstdErr);

    this->processId = os::Spawn(fullPath,arguments,environment,rstdIn,rstdOut,rstdErr);
}
CoExec::~CoExec()
{
    Kill();
}

bool CoExec::Wait(int timeoutMs)
{
    int result = -2;
    if (this->processId != os::ProcessId::Invalid)
    {
        result = os::WaitForProcess(processId, timeoutMs);
        this->processId = os::ProcessId::Invalid;
    }
    return result;
}
void CoExec::Kill()
{
    if (this->processId != os::ProcessId::Invalid)
    {
        os::KillProcess(this->processId);
        this->processId = os::ProcessId::Invalid;
    }
}
 