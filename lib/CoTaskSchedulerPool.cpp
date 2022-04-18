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

#include "CoTaskSchedulerPool.h"
#include <functional>
#include "ss.h"
#include "cotask/Os.h"

using namespace cotask;

CoTaskSchedulerPool::CoTaskSchedulerPool(CoDispatcher *pForegroundDispatcher)
    : pForegroundDispatcher(pForegroundDispatcher)
{
    int nCpus = std::thread::hardware_concurrency();
    if (nCpus == 0)
        nCpus = 3;
    int nThreads = nCpus - 1;
    if (nThreads < 1)
        nThreads = 1;
    Resize(nThreads);
}
CoTaskSchedulerPool::~CoTaskSchedulerPool()
{
    DestroyAllThreads();
    Log().Debug("CoTaskSchedulerPool deleted.");
    pForegroundDispatcher = nullptr;
}

void CoTaskSchedulerPool::ScavengeDeadThreads()
{
    if (this->deadThreads.size() != 0)
    {
        std::unique_lock lock{threadTerminatedMutex};
        for (auto thread : deadThreads)
        {
            delete thread;
            Log().Debug("Thread terminated.");
        }
        deadThreads.resize(0);
    }
}

void CoTaskSchedulerPool::DestroyAllThreads()
{
    {
        std::unique_lock lock{schedulerMutex};
        this->terminating = true;
        this->desiredSize = 0;
        readyToRun.notify_all();
    }

    while (true)
    {
        std::unique_lock lock{schedulerMutex};

        for (auto thread : deadThreads)
        {
            delete thread;
        }
        deadThreads.resize(0);

        if (threads.size() == 0)
            break;

        this->threadTerminatedCv.wait(lock);
    }
}

void CoTaskSchedulerPool::Resize(size_t desiredSize)
{
    {
        std::unique_lock lock{schedulerMutex};
        this->desiredSize = desiredSize;
        // if a reduction, just wait for threads to check desired size.
        for (size_t i = threads.size(); i < desiredSize; ++i)
        {
            ++threadSize;
            threads.push_back(new CoTaskSchedulerThread(this, pForegroundDispatcher));
        }
        readyToRun.notify_all();
    }
}

CoTaskSchedulerThread::CoTaskSchedulerThread(CoTaskSchedulerPool *pool, CoDispatcher *pForegroundDispatcher)
    : pool(pool),
      pForegroundDispatcher(pForegroundDispatcher)
{
    std::function<void(void)> fn = std::bind(&CoTaskSchedulerThread::ThreadProc, this);
    this->pThread = std::make_unique<std::jthread>(fn);
}

void CoTaskSchedulerThread::ThreadProc()
{
    try
    {
        os::SetThreadBackgroundPriority();
        CoDispatcher::pInstance = (new CoDispatcher(pForegroundDispatcher, this->pool));
        while (true)
        {
            auto h = pool->getOne(this);
            h.resume();
        }
    }
    catch (const TerminateException &ignored)
    {
    }
    catch (const std::exception &e)
    {
        pForegroundDispatcher->Log().Error(SS("Worker thread terminated abnormally. (" << e.what() << ")"));
    }
    pool->OnThreadTerminated(this);
    CoDispatcher::RemoveThreadDispatcher();
}

void CoTaskSchedulerPool::Post(std::coroutine_handle<> handle)
{
    std::unique_lock lock(schedulerMutex);

    handleQueue.push(handle);
    readyToRun.notify_one();
}

std::coroutine_handle<> CoTaskSchedulerPool::getOne(CoTaskSchedulerThread *pThread)
{
    std::unique_lock lock(schedulerMutex);
    pThread->isRunning = false; // done here, because operation has to be atomic under the schedulerMutex.

    while (true)
    {
        if (this->terminating)
        {
            --threadSize;
            throw TerminateException();
        }
        if (this->desiredSize < this->threadSize)
        {
            --threadSize;
            throw TerminateException();
        }
        if (!handleQueue.empty())
        {
            pThread->isRunning = true;
            return handleQueue.pop();
        }
        readyToRun.wait(lock);
    }
}

void CoTaskSchedulerPool::OnThreadTerminated(CoTaskSchedulerThread *thread)
{
    bool found = false;
    {
        std::unique_lock lock{schedulerMutex};
        for (auto i = threads.begin(); i != threads.end(); ++i)
        {
            if ((*i) == thread)
            {
                found = true;
                threads.erase(i);
                Log().Debug("Thread terminating.");
                break;
            }
        }
        if (found)
        {
            deadThreads.push_back(thread); // for cleanup.
            threadTerminatedCv.notify_one();
        }
    }
}
bool CoTaskSchedulerPool::IsDone()
{
    std::unique_lock lock{schedulerMutex};

    if (!handleQueue.empty())
        return false;
    for (auto thread : threads)
    {
        if (thread->isRunning)
            return false;
    }
    return true;
}
