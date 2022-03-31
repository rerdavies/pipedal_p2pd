#include "p2psession/Os.h"
#include <vector>
#include <stdexcept>
#include "ss.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

using namespace p2psession::os;
using namespace std;

static vector<string> split(const std::string &value, char splitChar)
{
    vector<string> result;

    size_t start = 0;

    while (start < value.size())
    {
        auto next = value.find_first_of(splitChar, start);
        if (next == string::npos)
        {
            result.push_back(value.substr(start));
            break;
        }
        result.push_back(value.substr(start, next - start));
        start = next + 1;
    }
    return result;
}

std::filesystem::path p2psession::os::FindOnPath(const std::string &filename)
{
    std::filesystem::path filePath{filename};
    if (filePath.is_absolute())
    {
        return filePath;
    }
    if (std::filesystem::exists(filePath))
    {
        return filePath;
    }
    vector<string> paths = split(getenv("PATH"), ':');

    for (auto path : paths)
    {
        filesystem::path file = std::filesystem::path(path) / filePath;
        if (filesystem::exists(file))
        {
            return file;
        }
    }
    throw std::invalid_argument(SS("File not found: " << filePath));
}

// see man 7 environ!
extern char **environ;

#define LEAVE_STDERRR 1
ProcessId p2psession::os::Spawn(
    const std::filesystem::path &path,
    const std::vector<std::string> &arguments,
    int stdinFileDescriptor,
    int stdoutFileDescriptor,
    int stderrFileDescriptor)
{
    // prepare the argument list before forking.
    const char **pArgs = new const char *[arguments.size() + 2];
    pArgs[0] = path.c_str();
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        pArgs[i] = arguments[i+1].c_str();
    }
    std::string dbgPath = os::FindOnPath("bash");
    pArgs[arguments.size()+1] = nullptr;

    pid_t pid = fork();
    if (pid != 0)
    {
        if (stdinFileDescriptor != -1)
            close(stdinFileDescriptor);
        if (stdoutFileDescriptor != -1)
            close(stdoutFileDescriptor);
        if (stderrFileDescriptor != -1)
            close(stderrFileDescriptor);

        delete[] pArgs;
        return (ProcessId)pid;
    }

    // new process....
    // dup stdio handles into place.
    if (stdinFileDescriptor != -1)
    {
        if (dup2(stdinFileDescriptor, 0) == -1)
        {
            cerr << "Error: Bad stdin file descriptor. " << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }
        close(stdinFileDescriptor);
    }
    if (stdoutFileDescriptor != -1)
    {
        if (dup2(stdoutFileDescriptor, 1) == -1)
        {
            cerr << "Error: Bad stdout file descriptor." << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }
        close(stdoutFileDescriptor);
    }
    if (stderrFileDescriptor != -1)
    {
        if (dup2(stderrFileDescriptor, 2) == -1)
        {
            cerr << "Error: Bad stderr file descriptor. " << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }
        close(stderrFileDescriptor);
    }

    int result = execv(dbgPath.c_str(), (char *const *)pArgs);
    cerr << "Error2: Exec failed." << strerror(errno) << endl;
    exit(EXIT_FAILURE);
}

void p2psession::os::msleep(int milliseconds)
{
    usleep(milliseconds);
}

void p2psession::os::KillProcess(ProcessId processId)
{
    pid_t pid = (pid_t)processId;

    // SIGTERM first. Give the process one second to terminate. If not...
    kill(pid, SIGTERM);

    int status;
    int ret = waitpid(pid, &status, WNOHANG);
    if (ret == -1)
        return; // no such pid.
    if (ret == pid)
        return;

    for (int i = 0; i < 10; ++i)
    {
        msleep(100);

        int ret = waitpid(pid, &status, WNOHANG);
        if (ret == -1)
            return; // no such pid.
        if (ret == pid)
            return;
    }

    ret = waitpid(pid, &status, WNOHANG);
    if (ret == -1)
        return; // no such pid.
    if (ret == pid)
        return;

    // SIGTERM didn't work. Kill the process.
    kill(pid, SIGKILL);
    ret = waitpid(pid, &status, 0);
    return;
}

bool p2psession::os::WaitForProcess(ProcessId processId, int timeoutMs)
{
    // waits for three seconds before giving up.
    pid_t pid = (pid_t)(uint64_t)processId; // actual platform-specific PID is stored in the underlying type of the enum.
    int status;
    if (timeoutMs < 0)
    {
        int ret = waitpid(pid, &status, 0);
    }
    else
    {
        int timeWaited = 0;
        for (int i = 0; i < 30; ++i)
        {

            int ret = waitpid(pid, &status, WNOHANG);
            if (ret == -1)
            {
                throw std::invalid_argument("No such process.");
            };
            if (ret == pid)
            {
                break;
            }
            if (timeWaited >= timeoutMs)
            {
                throw P2pTimeoutException();
            }

            msleep(100);
            timeWaited += 100;
        }
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status) == EXIT_SUCCESS;
    }
    return false; // terminated abnormally.
}

void p2psession::os::SetFileNonBlocking(int file_fd,bool nonBlocking)
{
    // remove NONBLOCKING.
    int statusFlags = fcntl(file_fd,F_GETFL);
    if (nonBlocking)
    {
        statusFlags |= O_NONBLOCK;
    } else {
        statusFlags &= ~O_NONBLOCK;
    }
    fcntl(file_fd,F_SETFL,statusFlags);
}
