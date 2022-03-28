#pragma once

#include <exception>
#include <stdexcept>
#include <filesystem>
#include "CoTask.h"
#include <chrono>
#include <stdexcept>
#include <condition_variable>
#include "CoService.h"
#include <bitset>

namespace p2psession
{
    class IoWaitable;

    class AsyncIoException : std::runtime_error
    {
    public:
        AsyncIoException(const std::string &what)
            : std::runtime_error("x"),
              errNo_(0)
        {
            what_ = what;
        }
        [[noreturn]] static void ThrowErrno();

        AsyncIoException(int errNo, const std::string &what)
            : std::runtime_error("x"), errNo_(errNo)
        {
            what_ = what;
        }
        virtual const char* what() const noexcept {
            return what_.c_str();
        }
        int errNo() const { return errNo_; }

    private:
        std::string what_;
        int errNo_;
    };

    class AsyncIo
    {
    protected:
        AsyncIo() {}
        ~AsyncIo() {}

    public:

        struct EventData {
            bool readReady;
            bool writeReady;
            bool hasError;
            bool hup;
        };

        using EventCallback = std::function<void (EventData eventdata)>;
        using EventHandle = uint64_t;




        static AsyncIo &GetInstance() { return *instance; }

        virtual EventHandle WatchFile(int fileDescriptor, EventCallback callback) = 0;
        virtual bool UnwatchFile(EventHandle handle) = 0;

    protected:
        static AsyncIo *instance;
    };



    // *******************************************************************************

};