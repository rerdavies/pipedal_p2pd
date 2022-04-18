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
#include "cotask/CoTask.h"
#include <thread>
#include <exception>
#include <coroutine>
#include <condition_variable>

#ifndef DOXYGEN
namespace cotask
{

    class TerminateException: std::exception {
    public:
        TerminateException() {

        }
        const char*what() const noexcept { return "Terminated.";}
    };

    class CoTaskSchedulerPool;

    class CoTaskSchedulerThread
    {
        friend class CoTaskSchedulerPool;

        CoTaskSchedulerThread(CoTaskSchedulerPool *pool, CoDispatcher *pForegroundDispatcher);
        

        void ThreadProc();

    private:
        bool isRunning = false;
        CoTaskSchedulerPool *pool;
        CoDispatcher *pForegroundDispatcher;
        std::unique_ptr<std::jthread> pThread;
    };
    class CoTaskSchedulerPool
    {
        friend class CoDispatcher;
        friend class CoTaskSchedulerThread;

        CoTaskSchedulerPool(CoDispatcher *pForegroundDispatcher);
        ~CoTaskSchedulerPool();

        std::coroutine_handle<> getOne(CoTaskSchedulerThread *pThread);

        void Resize(size_t threads);
        bool IsDone();
        void Post(std::coroutine_handle<> handle);
        void ScavengeDeadThreads();
    private:
        int runningTasks = 0;
        CoDispatcher *pForegroundDispatcher = nullptr;
        bool terminating = false;

        int desiredSize = 0;
        int threadSize = 0; // differs from thread.size() while a thread is in the process of terminating.
        std::vector<CoTaskSchedulerThread *> threads;
        std::vector<CoTaskSchedulerThread *> deadThreads;
        void DestroyAllThreads();
    
        ILog &Log() const { return pForegroundDispatcher->Log(); }


        std::mutex schedulerMutex;
        std::condition_variable readyToRun;

        void OnThreadTerminated(CoTaskSchedulerThread*thread);

        std::mutex threadTerminatedMutex;
        std::condition_variable threadTerminatedCv;
        Fifo<std::coroutine_handle<>> handleQueue;
    };

} // namespace

#endif