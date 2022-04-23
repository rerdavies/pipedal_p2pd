/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cotask/Os.h"
#include <vector>
#include <stdexcept>
#include "ss.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include "cotask/CoTask.h"
#include <uuid/uuid.h>

using namespace cotask;
using namespace cotask::os;
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

std::filesystem::path cotask::os::MakeTempFile()
{
    char templateName[] = "/tmp/co-XXXXXX";
    int fd = mkstemp(templateName);
    close(fd);
    return templateName;
}
std::filesystem::path cotask::os::FindOnPath(const std::string &filename)
{
    std::filesystem::path filePath{filename};
    if (filePath.is_absolute())
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
    // Files relative to the cwd. Require './path' for files in the cwd.
    if (filePath.has_parent_path())
    {
        if (std::filesystem::exists(filePath))
        {
            return filePath;
        }
    }

    throw CoFileNotFoundException(SS("File not found: " << filePath));
}

// see man 7 environ!
extern char **environ;

ProcessId cotask::os::Spawn(
    const std::filesystem::path &path,
    const std::vector<std::string> &arguments,
    const std::vector<std::string> &environment,
    int stdinFileDescriptor,
    int stdoutFileDescriptor,
    int stderrFileDescriptor)
{
    // prepare the argument list before forking.
    const char **pArgs = new const char *[arguments.size() + 2];
    // pArgs[0] = path.c_str();
    // for (size_t i = 0; i < arguments.size(); ++i)
    // {
    //     pArgs[i+1] = arguments[i].c_str();
    // }
    std::string fname = path.filename();
    pArgs[0] = fname.c_str();
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        pArgs[i+1] = arguments[i].c_str();
    }
    pArgs[arguments.size() + 1] = nullptr;

    const char **pEnv = new const char *[environment.size() + 1];
    for (size_t i = 0; i < environment.size(); ++i)
    {
        pEnv[i] = environment[i].c_str();
    }
    pEnv[environment.size()] = nullptr;

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
        delete[] pEnv;
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

    int result = execve(path.c_str(), (char *const *)pArgs, (char *const *)pEnv);
    if (result < 0)
    {
        cerr << "Error2: Exec failed." << strerror(errno) << endl;
    }
    exit(EXIT_FAILURE);
}

void cotask::os::msleep(int milliseconds)
{
    usleep(milliseconds);
}

void cotask::os::KillProcess(ProcessId processId)
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

bool cotask::os::WaitForProcess(ProcessId processId, int timeoutMs)
{
    // waits for three seconds before giving up.
    pid_t pid = (pid_t)(uint64_t)processId; // actual platform-specific PID is stored in the underlying type of the enum.
    int status;
    if (timeoutMs < 0)
    {
        int ret = waitpid(pid, &status, 0);
        if (ret == -1)
        {
            return true;
        };
    }
    else
    {
        int timeWaited = 0;
        for (int i = 0; i < 30; ++i)
        {

            int ret = waitpid(pid, &status, WNOHANG);
            if (ret == -1)
            {
                return true;
            };
            if (ret == pid)
            {
                break;
            }
            if (timeWaited >= timeoutMs)
            {
                throw CoTimedOutException();
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

void cotask::os::SetFileNonBlocking(int file_fd, bool nonBlocking)
{
    // remove NONBLOCKING.
    int statusFlags = fcntl(file_fd, F_GETFL);
    if (nonBlocking)
    {
        statusFlags |= O_NONBLOCK;
    }
    else
    {
        statusFlags &= ~O_NONBLOCK;
    }
    fcntl(file_fd, F_SETFL, statusFlags);
}

void cotask::os::SetThreadBackgroundPriority()
{
    std::ignore = nice(1);
}

namespace cotask::os
{
    std::string MakeUuid()
    {
        uuid_t uuid;
        uuid_generate(uuid);
        char strUid[36 + 1];
        uuid_unparse(uuid, strUid);
        return strUid;
    }
}
