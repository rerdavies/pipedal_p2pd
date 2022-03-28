#pragma once
#include "AsyncIo.h"
#include <functional>

namespace p2psession {
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

    class AsyncFile: private private_::WaitInterface
    {
    private:
        int file_fd = -1;
        AsyncIo::EventHandle eventHandle = -1;
        
        AsyncFile(const AsyncFile &){}; // no copy.
        AsyncFile( AsyncFile &&){}; // no move.
    public:
        enum OpenMode
        {
            Read,
            Create,
            Append
        };
        AsyncFile() { file_fd = -1; }

        AsyncFile(int file_fd);
        ~AsyncFile();

        void Close();

        Task<> CoOpen(const std::filesystem::path &path, AsyncFile::OpenMode mode);

        void  Attach(int fileDescriptor);
        int Detach();

        Task<int> CoRead(void *data, size_t length, std::chrono::milliseconds timeout = -1ms);

        Task<ssize_t> CoWrite(const void *data, size_t length, std::chrono::milliseconds timeout = -1ms);

        static void CreateSocketPair(AsyncFile &input, AsyncFile &output);
        static void CreateSocketPair(std::unique_ptr<AsyncFile>*input, std::unique_ptr<AsyncFile> *output)
        {
            (*input) = std::make_unique<AsyncFile>();
            (*output) = std::make_unique<AsyncFile>();
            CreateSocketPair(*input->get(),*output->get());
        }
        static void CreateSocketPair(std::shared_ptr<AsyncFile>*input, std::shared_ptr<AsyncFile> *output)
        {
            (*input) = std::make_shared<AsyncFile>();
            (*output) = std::make_shared<AsyncFile>();
            CreateSocketPair(*input->get(),*output->get());
        }

    private:
        std::mutex callbackMutex;
        
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