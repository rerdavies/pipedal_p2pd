#pragma once

#include "CoTask.h"
#include <vector>
#include <string>
#include "CoFile.h"
#include "Os.h"

namespace cotask
{

/**
 * @brief Execute a child process with standard I/O redirected to CoFile's.
 * 
 * Call Execute() to start the child process. Stdin() provides an CoFile that can be
 * used to write to the standard input of the child process from coroutines.
 * 
 * Stdout() and Stderr() allow coroutines to read standard output.
 */
    class CoExec
    {
    public:
        CoExec() {}
        virtual ~CoExec();

        /**
         * @brief Start a child process.
         * 
         * @param pathName Fully qualified path of the command to execute.
         * @param arguments Arguments for the child process.
         * 
         * After starting the process, a matching call should be made to Wait() in order to 
         * allow the zombie process that is left after the child process terminates to be reclaimed.
         * 
         * The child inherits environment variables from the current process.
         */
        void Execute(
            const std::filesystem::path &pathName,
            const std::vector<std::string> &arguments);
        /**
         * @brief Start a child process.
         * 
         * @param pathName Fully qualified path of the command to execute.
         * @param arguments Arguments for the child process.
         * @param environment Environment variables for the chill process.
         * 
         * After starting the process, a matching call should be made to Wait() in order to 
         * allow the zombie process that is left after the child process terminates to be reclaimed.
         * 
         */
        void Execute(
            const std::filesystem::path &pathName,
            const std::vector<std::string> &arguments,
            const std::vector<std::string> &environment);

        /**
         * @brief Wait for the child process to terminate.
         * 
         * @param timeoutMs (Optional) How long to wait.
         * @return true if the process terminated normally.
         * @return false if the process terminated abnormally.
         * 
         * If the operation times out, a CoTimeoutException() is thrown.
         * 
         */
        CoTask<bool> CoWait(int timeoutMs = -1);

        /**
         * @brief Syncrhonously wait for the child process to terminate.
         * 
         * @param timeoutMs (Optional) How long to wait.
         * @return true if the process terminated normally.
         * @return false if the process terminated abnormally.
         * 
         * If the operation times out, a CoTimeoutException() is thrown.
         * 
         */
        bool Wait(int timeoutMs = -1);

 
        /**
         * @brief Has the child process terminated?
         * 
         * @return true child has terminated.
         * @return false child is still running.
         */
        bool HasTerminated() const;
        /**
         * @brief Terminate the child process.
         * 
         * CoKill() attempts a graceful shutdown by sending a SIGTRM signal. If the child doesn't respond,
         * within three seconds, CoKill() sends a SIGKILL signal.
         */
        CoTask<> CoKill();

        /**
         * @brief Terminate the child process.
         * 
         * Kill() attempts a graceful shutdown by sending a SIGTRM signal. If the child doesn't respond,
         * within three seconds, Kill() sends a SIGKILL signal.
         */
        void Kill();

        /**
         * @brief Standard input for the child process.
         * 
         * @return CoFile& 
         */
        CoFile & Stdin() { return stdin; }
        /**
         * @brief Standard output for the child process.
         * 
         * @return CoFile& 
         */
        CoFile & Stdout() { return stdout; }
        /**
         * @brief Stander error outut for the child process.
         * 
         * @return AsyncFile& 
         */
        CoFile & Stderr() { return stderr; }
    private:
        os::ProcessId processId = os::ProcessId::Invalid;
        CoFile stdin, stdout, stderr;
    };
} // namespace