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

        virtual void Start() = 0;
        virtual void Stop() = 0;

    protected:
        static AsyncIo *instance;
    };

    // *******************************************************************************

}