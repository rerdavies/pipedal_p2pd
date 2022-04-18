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

    class CoExecException : public CoException {
    public:
        CoExecException() { }
        virtual const char*what() const noexcept {
            return "Program terminated abnormally.";
        }
    };

    class CoExec
    {
    public:
        CoExec() {}
        virtual ~CoExec();

        /**
         * @brief Execute a program, waiting for result.
         * 
         * Waits for the child process to complete.
         * 
         * @param pathName The program to execute.
         * @param arguments Arguments to supply to the program.
         * @param output (optional) On exit, contains the output of the program.
         * @returns true if the program exited successfully.
         * @returns false if the program exited unsuccessfully, or terminated abnormally.
         * @throws CoIoException if the program can't be found.
         * 
         */
        CoTask<bool> CoExecute(
            const std::filesystem::path &pathName,
            const std::vector<std::string> &arguments,
            std::string *output);

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
         * After starting the process, a matching call should be made to either Wait() or CoWait() in order to 
         * allow the zombie process that is left after the child process terminates to be reclaimed. 
         * 
         * Zombie processes are recycled after the current process terminates. Failing to call Wait() or CoWait()
         * leaks memory on both Windows and Linux, and may cause memory exhaustion in long-running processes.
         * 
         */
        void Execute(
            const std::filesystem::path &pathName,
            const std::vector<std::string> &arguments,
            const std::vector<std::string> &environment);

        /**
         * @brief Syncrhonously wait for the child process to terminate.
         * 
         * @param timeoutMs (Optional) How long to wait.
         * @return true if the process terminated normally.
         * @return false if the process terminated abnormally.
         * @throws CoTimeoutException if a timeout occurs.
         * 
         * The normal procedure for running programs is to either read Stdout() and Stderror() until
         * both are closed, and then call Wait() (which won't block) in order to remove the zombie process.
         * 
         * If you are not interested in the output, call DiscardOutput().
         * 
         * Wait() returns immediately if no process was started.
         * 
         */
        bool Wait(std::chrono::milliseconds timeoutMs = NO_TIMEOUT);

 
        /**
         * @brief Wait for the child process to terminate.
         * 
         * @param timeoutMs (Optional) How long to wait.
         * @return true if the process terminated normally.
         * @return false if the process terminated abnormally.
         * @throws CoTimeoutException if a timeout occurs.
         * 
         * CoWait() polls for the process to terminate. It's more efficient
         * to read standard outputs until both outputs signal end-of-file 
         * before calling CoWait().
         * 
         * Wait() returns immediately if no process was started.
         * 
         */
        CoTask<bool> CoWait(std::chrono::milliseconds timeoutMs = NO_TIMEOUT);
        
        /**
         * @brief Discard all present and future output on Stdout and Stderr.
         * 
         * Spawns child thread to silently read and discard output from the child process.
         * 
         * This prevents the child process from blocking if output buffers fill; and also
         * allows CoWait() to operate with maximum efficiency. The implementation calls 
         * DiscardOutput() on Stdout() and Stderr().
         */
        void DicardOutputs() { DiscardOutput(Stdout()); DiscardOutput(Stderr()); }


        /**
         * @brief Silently discards output from either Stdout() or Stderr().
         * 
         * The implementation spawns a CoTask<> that reads and discards output until
         * end of file is reached.
         * 
         * @param file Either Stdout(), or Stderr().
         * @throws std::invalid_argument if file is not one of StdOut() or Stderr().
         * 
         */
        void DiscardOutput(CoFile &file);


        /**
         * @brief Has the child process terminated?
         * 
         * @return true child has terminated.
         * @return false child is still running.
         */
        bool HasTerminated() const;
        /**
         * @brief Terminate the child process and wait for completion.
         * 
         * CoKill() attempts a graceful shutdown by sending a SignalType::interrupt signal. If the child doesn't respond,
         * within the grace period, CoKill() sends a Signal::Kill signal. In either case, CoWait() will have been called 
         * on return. 
         * 
         * @param gracePeriod the time to wait for the process to respond to Kill(SignalType::Interrupt. (Default 3000ms).

         */
        CoTask<> CoKill(std::chrono::milliseconds gracePeriod = std::chrono::milliseconds(3000));

        /**
         * @brief The urgency with which Signal() should be handled by the child process.
         * 
         * Implementation is platform specific; but the general intent is that Interrupt and Terminate 
         * should trigger a graceful shutdoewn, whereas Kill should terminate the process as expediently
         * as the platform allows.
         * 
         */
        enum class SignalType {
            Interrupt,  // equivalent of ^C/SIGINT
            Terminate,  // Equivalent of SIGTERM (or ^C if there's no distinction for the current platform).
            Kill,       // equivalent of KILL/TermianteProcess().
        };

        /**
         * @brief Send a signal to the child process instructing it to stop running.
         * 
         * This is a non-blocking call. Send a  the process. Kill(SignalType::Kill) should not be assumed to be efficient enough
         * that Wait() won't block. Use CoWait(), or CoRead() on Stdout() and Stderr() until both reach end-of-file.
         * 
         * Precise implementation is platform-dependent, but *must* conform to Posix/ISO C99 semantics.
         * 
         * @param signalType 
         */
        void Kill(SignalType signalType);

        
        /**
         * @brief Standard input for the child process.
         * 
         * @return a CoFile& 
         */
        CoFile & Stdin() { return stdin; }
        /**
         * @brief Standard output for the child process.
         * 
         * @return a CoFile& 
         */
        CoFile & Stdout() { return stdout; }
        /**
         * @brief Stander error outut for the child process.
         * 
         * @return AsyncFile& 
         */
        CoFile & Stderr() { return stderr; }
    private:
        bool exitResult = true;
        CoTask<> OutputReader(CoFile &file,std::ostream&stream);
        CoTask<> DiscardingOutputReader(CoFile&file);

        int activeOutputs = 0;
        CoConditionVariable cvOutput;

        os::ProcessId processId = os::ProcessId::Invalid;
        CoFile stdin, stdout, stderr;
    };
} // namespace