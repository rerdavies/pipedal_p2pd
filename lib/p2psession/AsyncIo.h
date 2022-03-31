#pragma once

#include <exception>
#include <stdexcept>
#include <filesystem>
#include "CoTask.h"
#include <chrono>
#include <stdexcept>
#include <condition_variable>
#include "CoService.h"
#include "Log.h"

namespace p2psession
{
    class IoWaitable;

    /**
     * @brief An i/o exception.
     * 
     * ErrNo() contains an errno error code.
     */
    class AsyncIoException : std::runtime_error
    {
    public:
        /**
         * @brief Throw an AsyncIoException using the current value of errno.
         * 
         */
        [[noreturn]] static void ThrowErrno();

        /**
         * @brief Construct a new Async Io Exception object
         * 
         * @param errNo an errno error code.
         * @param what text for the error.
         */
        AsyncIoException(int errNo, const std::string &what)
            : std::runtime_error("x"), errNo_(errNo)
        {
            what_ = what;
        }
        virtual const char *what() const noexcept
        {
            return what_.c_str();
        }
        int errNo() const { return errNo_; }

    private:
        std::string what_;
        int errNo_;
    };

    /**
     * @brief End of file exception.
     * 
     */
    class AsyncEndOfFileException : public AsyncIoException
    {
    public:
        /**
         * @brief Construct a new Async End Of File Exception object
         * 
         */
        AsyncEndOfFileException() : AsyncIoException(ENODATA,"End of file.") {}
    };

    /**
     * @brief Private implementation of asynchronous I/O notifications.
     * 
     *  Private use. Methods are implementation (and probably) platfrom dependent.
     */
    class AsyncIo
    {
    protected:
        AsyncIo() {}
        ~AsyncIo() {}

    public:
        struct EventData
        {
            bool readReady;
            bool writeReady;
            bool hasError;
            bool hup;
        };

        using EventCallback = std::function<void(EventData eventdata)>;
        using EventHandle = uint64_t;

        static AsyncIo &GetInstance() { return *instance; }

        virtual EventHandle WatchFile(int fileDescriptor, EventCallback callback) = 0;
        virtual bool UnwatchFile(EventHandle handle) = 0;

    protected:
        static AsyncIo *instance;
    };

    // *******************************************************************************

};