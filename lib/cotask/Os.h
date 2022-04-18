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

#include <filesystem>
#include <string>
#include <vector>
#include "CoExceptions.h"

namespace cotask
{
    namespace os
    {
        // Opaque handle for a process. Use ProcessId::Invalid for uninitialized values.
        enum class ProcessId : int64_t
        {
            Invalid = 0
        };

        /**
         * @brief Find the fully-qualifed path of an executable.
         * 
         * @param filename 
         * @return std::filesystem::path
         * @throws CoFileNotFoundException if the file doesn't exist and can't be found.
         * 
         * Searches the platform-appropriate path for the named executabe. Fully-qualified
         * paths are returned unmodified, but CoFileNotFoundException is thrown if the file doesn't exist.
         */
        std::filesystem::path FindOnPath(const std::string &filename);

        /**
         * @brief Get the fully-qualified path for a temporary file.
         * 
         * On return, the unique file will have been created, but will not have been opened.
         * 
         * @return std::filesystem::path 
         */
        std::filesystem::path MakeTempFile();

        /**
         * @brief Spawn a child process.
         * 
         *    Throws an exception on failure. Passing a -1 value for file descriptors causes them 
         *    to not be redirected.
         * 
         * @param path Fully-qualified path of the executable to run.
         * @param arguments Command-line arguments.
         * @param stdinFileDescriptor File descriptor for Child's stdin file 
         * @param stdoutFileDescriptor File descriptor for Child's stdout file 
         * @param stderrFileDescriptor File descriptor for Child's stderr file 
         * @return ProcessId 
         */
        ProcessId Spawn(
            const std::filesystem::path &path,
            const std::vector<std::string> &arguments,
            const std::vector<std::string> &environment,
            int stdinFileDescriptor = -1,
            int stdoutFileDescriptor = -1,
            int stderrFileDescriptor = -1);
        void KillProcess(ProcessId processId);
        /**
         * @brief Wait for process to exit.
         * 
         * @param processId Id of the process.
         * @param timeoutMs Timeout in milliseconds. -1 to wait indefinitely, 0 to poll.
         * @return true - Processed exited normally.
         * @return false - Process exited abnormally.
         * @throws OsException if a timeout occurs.
         */
        bool WaitForProcess(ProcessId processId, int timeoutMs = -1);

        /**
         * @brief Sleep for milliseconds.
         * 
         * @param milliseconds 
         *     The number of milliseconds to sleep for. If value is less than zero, sleep 
         *     for at least one scheduling quantum.
         */
        void msleep(int milliseconds);

        void SetFileNonBlocking(int file_fd, bool nonBlocking);

        /**
         * @brief Cause the current thread to run with background scheduling priority.
         * 
         * @return * void 
         */
          
        void SetThreadBackgroundPriority();

        std::string MakeUuid();

    }
}