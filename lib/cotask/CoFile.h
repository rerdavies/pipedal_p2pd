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
#include "AsyncIo.h"
#include <functional>
#include <string>
#include <sstream>
#include "CoEvent.h"


namespace cotask {

    /**
     * @brief A file that can be read asynchronously by coroutines.
     * 
     */
    class CoFile
    {
    private:
        int file_fd = -1;
        AsyncIo::EventHandle eventHandle = -1;
        
        CoFile(const CoFile &){}; // no copy.
        CoFile( CoFile &&){}; // no move.
    public:
        enum class OpenMode
        {
            Read,
            Create,
            Append,
            ReadWrite
        };
        /**
         * @brief Default constructor.
         * 
         */
        CoFile() { file_fd = -1; }

        /**
         * @brief Construct an AsyncFile from a file handle.
         * 
         * The AsyncFile takes ownership of the file_fd. 
         * 
         * @param file_fd 
         */
        CoFile(int file_fd);
        ~CoFile();

        /**
         * @brief Close the file.
         * 
         */
        CoTask<> CoClose();

        /**
         * @brief Closes the file, synchronously waiting for all i/o operations to complete.
         * 
         * CoClose pumps messages on the current CoDispatcher until all i/o operations have settled. Prefer CoClose() if you can.
         * 
         */
        void Close();
        /**
         * @brief Open a file.
         * 
         * @param path The filesystem path of the file to open.
         * @param mode Read, Write, or Append.
         * @return Task<> 
         */
        CoTask<> CoOpen(const std::filesystem::path &path, CoFile::OpenMode mode);

        /**
         * @brief Atach a file handle and take ownership of it.
         * 
         * @param fileDescriptor A file handle.
         * 
         * The current file (if any) will be closed. 
         */
        void  Attach(int fileDescriptor);
        /**
         * @brief Return the file handle for the file and relinquish ownership of it.
         * 
         * @return int A file handle. -1, if the AysncFile is not open.
         */
        int Detach();

        /**
         * @brief Is the file open?
         * 
         * @return true if the file is open.
         * @return false  if the file is closed.
         */
        bool IsOpen() const { return this->file_fd != -1; }
        /**
         * @brief Read a buffer of data.
         * 
         * @param data The buffer into which to read.
         * @param length The maximum number of bytes to read.
         * @param timeout Timeout in milliseconds.
         * @return Task<int> The number of bytes read. 0 on end of file.
         * 
         * The return value indicates how many bytes were read. Generally, this method does not return a full buffer; if there is any
         * data waiting in the file, it is returned immediately, whether the buffer is full or not.
         * 
         * A CoTimeoutException is thrown if the operatin times out. The state of the read data is determinated. Partially read data 
         * will be returned before the exception is thrown.
         * 
         */
        CoTask<size_t> CoRead(void *data, size_t length, std::chrono::milliseconds timeout = -1ms);

        /**
         * @brief Receive a datagram of data.
         * 
         * @param data The buffer into which to read.
         * @param length The maximum number of bytes to read.
         * @param timeout Timeout in milliseconds.
         * @return Task<int> The number of bytes read. 0 on end of file.
         * 
         * The return value indicates how many bytes were read. Generally, this method does not return a full buffer; if there is any
         * data waiting in the file, it is returned immediately, whether the buffer is full or not.
         * 
         * A CoTimeoutException is thrown if the operation times out. The state of the read data is determinated. Partially read data 
         * will be returned before the exception is thrown.
         * 
         * The principle difference between CoRead() and CoRecv() is that CoRecv() expecteds datagrams, including 
         * zero-length datagrams, wherease CoRead()'s behaviour is formally undefined if a zero-length
         * datagram is received; and CoRead will attempt to combine multiple packets into a single read response if multiple 
         * packets are available, whereas CoRecv will never coallesce packets. However, there may be other differences as well. To be 
         * safe, use CoRecv for datagram sockets.
         * 
         */
        CoTask<size_t> CoRecv(void *data, size_t length, std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Read a line of data.
         * 
         * @param result The line of data, not including the trailing '\n';
         * @return true Success.
         * @return false End of file.
         */
        CoTask<bool> CoReadLine(std::string*result);

        /**
         * @brief Write a buffer of data.
         * 
         * @param data The buffer of data to write.
         * @param length The number of bytes to write.
         * @param timeout (optional) Maximum time to wait for the write to complete.
         * @return Task<> 
         * @throws CoTimeoutException
         * 
         * Always attempts to write the entire buffer. If a timeout occurs, a CoTimeoutException is thrown. After a timeout, the amount of data written 
         * is indeterminate.
         * 
         */
        CoTask<> CoWrite(const void *data, size_t length, std::chrono::milliseconds timeout = -1ms);

        /**
         * @brief Send data on a socket.
         * 
         * @param data The buffer of data to write.
         * @param length The number of bytes to write (may be zero).
         * @param timeout (optional) Maximum time to wait for the write to complete.
         * @return Task<> 
         * @throws CoTimeoutException
         * 
         * Write a datagram of data on a socket.
         * 
         * The principle difference between CoWrite, and CoSend is that CoSend will correctly
         * handle zero-length datagrams; but the behavior of CoWrite is undefined in this case. But 
         * there may be other differences as well. 
         * 
         * If a write with length of zero is made, CoWrite returns immediately without
         * performing an operation on the underlying file handle. 
         * 
         * On linux, CoWrite calls write(), whereas CoSend() calls send().
         */
        CoTask<> CoSend(const void *data, size_t length, std::chrono::milliseconds timeout = -1ms);



        /**
         * @brief Write a string.
         * 
         * @param text The string to write.
         * @param timeout (optiona) Maximum time to wait for the write to complete.
         * @return Task<> 
         * @throws CoTimeoutException
         * 
         * If a timeout occurs, a CoTimeoutException is thrown. After a timeout, the amount of data written 
         * is indeterminate.
         */
        CoTask<> CoWrite(const std::string & text, std::chrono::milliseconds timeout = NO_TIMEOUT)
        {
            co_await CoWrite(text.c_str(),text.length(), timeout);
            co_return;
        }
        /**
         * @brief Write a string followed by '\n';
         * 
         * @param text The string to write.
         * @param timeout (optional) Maximum time to wait for the write to complete.
         * @return Task<> 
         * @throws CoTimeoutException
         * 
         * If a timeout occurs, a CoTimeoutException is thrown. After a timeout, the amount of data written 
         * is indeterminate.
         */
        CoTask<> CoWriteLine(const std::string &text, std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Open the supplied AsyncFiles as a pair of connected anonymous pipes.
         * 
         * @param input 
         * @param output 
         * 
         * @throws CoIoException
         */
        static void CreateSocketPair(CoFile &input, CoFile &output);
        /**
         * @brief Create a pair of std::unique_ptr<AsyncFile>'s that are connected anonymous pipes.
         * 
         * @param input 
         * @param output 
         * 
         * @throws CoIoException
         */

        static void CreateSocketPair(std::unique_ptr<CoFile>*input, std::unique_ptr<CoFile> *output)
        {
            (*input) = std::make_unique<CoFile>();
            (*output) = std::make_unique<CoFile>();
            CreateSocketPair(*input->get(),*output->get());
        }
        /**
         * @brief Create a pair of std::shard_ptr<AsyncFile>'s that are connected anonymous pipes.
         * 
         * @param input 
         * @param output 
         * 
         * @throws CoIoException
         */
        static void CreateSocketPair(std::shared_ptr<CoFile>*input, std::shared_ptr<CoFile> *output)
        {
            (*input) = std::make_shared<CoFile>();
            (*output) = std::make_shared<CoFile>();
            CreateSocketPair(*input->get(),*output->get());
        }

    private:

        int lineHead = 0;
        int lineTail = 0;
        char lineBuffer[512];
        std::stringstream lineSS;

        bool deleted = false;
        bool readReady = false;
        bool writeReady = false;

        CoConditionVariable readCv;
        CoConditionVariable writeCv;


        bool closed = true;
        bool closing = false;
        int pendingOperations = 0;
        CoConditionVariable closeCv;
        void CheckUseAfterDelete();

        class OpsLock {
        public:
            OpsLock();

            OpsLock(CoFile *pFile) : pFile(pFile) {
                if (pFile->deleted) // catch *some* but not all use-after-free errors.
                {
                    Terminate("Use after free!");
                }
                pFile->closeCv.Execute([this] () {
                    this->pFile->pendingOperations++;
                });
            }
            ~OpsLock() {
                pFile->closeCv.Notify([this] () {
                    this->pFile->pendingOperations--;
                });
            }
        private:
            CoFile *pFile;
        };

        void WatchFile(int fd);

    };


} // namespace