#pragma once
#include "AsyncIo.h"
#include <functional>
#include <string>
#include <sstream>


namespace cotask {
    #ifndef DOXYGEN
    namespace private_ {
        enum WaitOperation {
            Read,
            Write
        };
        class WaitInterface {
        public:
            using callback = std::function<void(void)>;
            virtual bool IsReady(WaitOperation op) = 0;
            virtual void SetReadyCallback(WaitOperation op,callback callbackOnReady) = 0;
            virtual void ClearReadyCallback(WaitOperation op) = 0;
        };
    };
    #endif

    /**
     * @brief A file that can be read asynchronously by coroutines.
     * 
     */
    class CoFile: private private_::WaitInterface
    {
    private:
        int file_fd = -1;
        AsyncIo::EventHandle eventHandle = -1;
        
        CoFile(const CoFile &){}; // no copy.
        CoFile( CoFile &&){}; // no move.
    public:
        enum OpenMode
        {
            Read,
            Create,
            Append
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
        CoTask<int> CoRead(void *data, size_t length, std::chrono::milliseconds timeout = -1ms);
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
         */
        CoTask<> CoWrite(const void *data, size_t length, std::chrono::milliseconds timeout = -1ms);


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
        std::mutex readMutex;
        std::mutex writeMutex;
        std::mutex callbackMutex;

        int lineHead = 0;
        int lineTail = 0;
        char lineBuffer[512];
        std::stringstream lineSS;
        
        virtual bool IsReady(private_::WaitOperation op);
        virtual void SetReadyCallback(private_::WaitOperation op,private_::WaitInterface::callback callbackOnReady);
        virtual void ClearReadyCallback(private_::WaitOperation op);

        private_::WaitInterface::callback readCallback;
        private_::WaitInterface::callback writeCallback;
        bool readReady = false;
        bool writeReady = false;
        void WatchFile(int fd);

    };


} // namespace