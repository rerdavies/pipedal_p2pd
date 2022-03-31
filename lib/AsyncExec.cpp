#include "p2psession/AsyncExec.h"
#include "p2psession/Os.h"
#include <fcntl.h>

using namespace p2psession;
using namespace p2psession::os;

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
void AsyncExec::Execute(
    const std::filesystem::path &pathName,
    const std::vector<std::string> &arguments
    )
{
    auto fullPath = FindOnPath(pathName);
    AsyncFile remoteStdin,remoteStdout,remoteStderr;

    AsyncFile::CreateSocketPair(this->stdin,remoteStdin);
    AsyncFile::CreateSocketPair(this->stdout,remoteStdout);
    //AsyncFile::CreateSocketPair(this->stderr,remoteStderr);

    int rstdIn = remoteStdin.Detach();
    PrepareFile(rstdIn);
    int rstdOut = remoteStdout.Detach();
    PrepareFile(rstdOut);
    int rstdErr = remoteStderr.Detach();
    PrepareFile(rstdErr);

    this->processId = os::Spawn(fullPath,arguments,rstdIn,rstdOut);
}
AsyncExec::~AsyncExec()
{
    Kill();
}

bool AsyncExec::Wait(int timeoutMs)
{
    int result = -2;
    if (this->processId != os::ProcessId::Invalid)
    {
        result = os::WaitForProcess(processId, timeoutMs);
        this->processId = os::ProcessId::Invalid;
    }
    return result;
}
void AsyncExec::Kill()
{
    if (this->processId != os::ProcessId::Invalid)
    {
        os::KillProcess(this->processId);
        this->processId = os::ProcessId::Invalid;
    }
}
 