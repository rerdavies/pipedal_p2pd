#pragma once 

#include <filesystem>
#include <string>
#include <vector>
#include "P2pException.h"

namespace p2psession {
    namespace os {

        // Opaque handle for a process. Use ProcessId::Invalid for uninitialized values.
        enum class ProcessId : int64_t { Invalid=0}; 
        std::filesystem::path FindOnPath(const std::string & filename);

/**
 * @brief Spawn a child process.
 * 
 * @param path Fully-qualified path of the executable to run.
 * @param arguments Command-line arguments.
 * @param stdinFileDescriptor File descriptor for Child's stdin file 
 * @param stdoutFileDescriptor File descriptor for Child's stdout file 
 * @param stderrFileDescriptor File descriptor for Child's stderr file 
 * @return ProcessId 
 * @remarks Throws an exception on failure. Passing a -1 value for file descriptors causes them 
 *     to not be redirected.
 */
        ProcessId Spawn(
            const std::filesystem::path &path,
            const std::vector<std::string> &arguments,
            int stdinFileDescriptor = -1,
            int stdoutFileDescriptor = -1,
            int stderrFileDescriptor = -1
            );
        void KillProcess(ProcessId processId);
        /**
         * @brief Wait for process to exit.
         * 
         * @param processId Id of the process.
         * @param timeoutMs Timeout in milliseconds. -1 to wait indefinitely, 0 to poll.
         * @return true - Processed exited normally.
         * @return false - Process exited abnormally.
         * @remarks Throws P2pTimeoutException if a timeout occurs.
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

        void SetFileNonBlocking(int file_fd,bool nonBlocking);

    };
}