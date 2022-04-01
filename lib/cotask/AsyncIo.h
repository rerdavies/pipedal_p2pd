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
#include "CoExceptions.h"

namespace cotask
{
    class IoWaitable;

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