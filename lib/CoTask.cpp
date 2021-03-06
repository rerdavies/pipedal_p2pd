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


#include "cotask/CoTask.h"
#include <chrono>
#include "ss.h"
#include "CoTaskSchedulerPool.h"
#include "cotask/Os.h"

using namespace cotask;
using namespace std;

static constexpr bool debugTimers = false;
using Clock = std::chrono::steady_clock;

thread_local CoDispatcher *CoDispatcher::pInstance;
CoDispatcher *CoDispatcher::gForegroundDispatcher;
std::mutex CoDispatcher::gLogMutex;


CoDispatcher::TimeMs CoDispatcher::Now()
{

    auto now = Clock::now();
    auto duration = now.time_since_epoch();

    return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}

void CoDispatcher::PostDelayed(TimeMs delay, const std::coroutine_handle<> &handle)
{
    if (!IsForeground())
    {
        pForegroundDispatcher->PostDelayed(delay, handle);
    }
    else
    {
        {
            std::lock_guard lock{this->schedulerMutex};
            CoroutineTimerEntry entry{Now() + delay, handle};
            coroutineTimerQueue.push(std::move(entry));
        }
        PumpMessageNotifyOne();
    }
}
uint64_t CoDispatcher::PostDelayedFunction(TimeMs delay, std::function<void(void)> callback)
{
    if (!IsForeground())
    {
        return pForegroundDispatcher->PostDelayedFunction(delay, callback);
    }
    else
    {
        uint64_t handle;
        {
            std::lock_guard lock{this->schedulerMutex};
            handle = ++nextTimerHandle;
            TimerFunctionEntry entry{Now() + delay, callback, handle};
            bool inserted = false;
            if (debugTimers) Log().Debug(SS("insert fn " << functionTimerQueue.size() << "  "));
            for (auto i = functionTimerQueue.begin(); i != functionTimerQueue.end(); ++i)
            {
                if (entry.time.count() < i->time.count())
                {
                    functionTimerQueue.insert(i, std::move(entry));
                    inserted = true;
                    break;
                }
            }
            if (!inserted)
            {
                functionTimerQueue.push_back(entry);
            }
            if (debugTimers) Log().Debug(SS("...." << functionTimerQueue.size()));
        }
        PumpMessageNotifyOne();
        return handle;
    }
}

bool CoDispatcher::CancelDelayedFunction(uint64_t handle)
{
    if (!IsForeground())
    {
        return pForegroundDispatcher->CancelDelayedFunction(handle);
    }
    std::lock_guard lock{this->schedulerMutex};

    for (auto i = functionTimerQueue.begin(); i != functionTimerQueue.end(); ++i)
    {
        if (i->timerHandle == handle)
        {
            functionTimerQueue.erase(i);
            if (debugTimers) Log().Debug(SS("fn timer cancelled: " << functionTimerQueue.size()));
            return true;
        }
    }
    return false;
}

void CoDispatcher::PostBackground(std::coroutine_handle<> handle)
{
    this->pSchedulerPool->Post(handle);
}

void CoDispatcher::Post(std::coroutine_handle<> handle)
{
    if (!IsForeground())
    {
        pForegroundDispatcher->Post(handle);
    }
    else
    {
        {
            std::unique_lock lock{schedulerMutex};
            queue.push(handle);
        }
        PumpMessageNotifyOne();
    }
}

void CoDispatcher::PumpUntilIdle()
{
    if (!IsForeground())
    {
        throw logic_error("Can't perform this operation on a background thread.");
        return;
    }

    while (!IsDone())
    {
        PumpMessages();
    }
}

void CoDispatcher::MessageLoop(CoTask<> &&threadMain)
{
    StartThread(std::forward<CoTask<>>(threadMain));
    MessageLoop();
}
void CoDispatcher::MessageLoop()
{
    if (!IsForeground())
    {
        throw std::logic_error("Not on foreground thread.");
    }
    if (inMessageLoop)
    {
        throw std::logic_error("Already running a message loop.");
    }
    inMessageLoop = true;
    while (true)
    {
        PumpMessages(true);
        if (quit)
        {
            break;
        }
        PumpMessageWaitOne();
    }
    inMessageLoop = false;
    quit = false; // allow MessageLoop to run again.
}

void CoDispatcher::SetThreadPoolSize(size_t size)
{
    if (!IsForeground())
    {
        throw std::invalid_argument("Not allowed on a background thread.");
    }
    pSchedulerPool->Resize(size);
}

bool CoDispatcher::PumpTimerMessages(TimeMs time)
{
    std::unique_lock<std::mutex> lock{schedulerMutex};

    if (coroutineTimerQueue.empty())
    {
        if (functionTimerQueue.empty())
        {
            return false;
        }
        else
        {
            if (functionTimerQueue.begin()->time <= time)
            {
                auto fn = functionTimerQueue.begin()->fn;

                if (debugTimers) Log().Debug(SS("fn executed: " << functionTimerQueue.size() << "..."));

                functionTimerQueue.pop_front();

                if (debugTimers) Log().Debug(SS("...." << functionTimerQueue.size()));

                lock.unlock();
                fn();

                return true;
            }
            return false;
        }
    }
    else
    {
        if (functionTimerQueue.empty())
        {
            if (coroutineTimerQueue.top().time <= time)
            {
                auto handle = coroutineTimerQueue.top().handle;
                coroutineTimerQueue.pop();
                lock.unlock();
                handle.resume();
                return true;
            }
            return false;
        }
        else
        {
            if (functionTimerQueue.front().time.count() <= coroutineTimerQueue.top().time.count())
            {
                if (functionTimerQueue.front().time <= time)
                {
                    auto fn = functionTimerQueue.front().fn;

                    if (debugTimers) Log().Debug(SS("fn executed: " << functionTimerQueue.size() << "..."));

                    functionTimerQueue.pop_front();

                    if (debugTimers) Log().Debug(SS("...." << functionTimerQueue.size()));

                    lock.unlock();
                    fn();
                    return true;
                }
            }
            else if (coroutineTimerQueue.top().time.count() < time.count())
            {
                if (coroutineTimerQueue.top().time <= time)
                {
                    auto handle = coroutineTimerQueue.top().handle;
                    coroutineTimerQueue.pop();
                    lock.unlock();
                    handle.resume();
                    return true;
                }
            }
            return false;
        }
    }
}

bool CoDispatcher::GetNextTimer(CoDispatcher::TimeMs *pResult) const
{
    if (this->coroutineTimerQueue.empty())
    {
        if (this->functionTimerQueue.empty())
        {
            return false;
        }
        else
        {
            *pResult = this->functionTimerQueue.front().time;
            return true;
        }
    }
    else
    {
        if (this->functionTimerQueue.empty())
        {
            *pResult = this->coroutineTimerQueue.top().time;
            return true;
        }
        else
        {
            auto t1 = this->coroutineTimerQueue.top().time;
            auto t2 = this->functionTimerQueue.front().time;
            if (t1 < t2)
            {
                *pResult = t1;
            }
            else
            {
                *pResult = t2;
            }
            return true;
        }
    }
}

void CoDispatcher::SleepFor(TimeMs delay)
{
    SleepUntil(Now() + delay);
}
void CoDispatcher::SleepUntil(TimeMs time)
{
    if (IsForeground())
    {

        while (true)
        {
            auto now = Now();
            if (now >= time)
                break;

            TimeMs waitTime = time;
            TimeMs timerDelay;
            if (GetNextTimer(&timerDelay))
            {
                if (timerDelay < time)
                {
                    waitTime = timerDelay;
                }
            }
            {
                std::unique_lock lock{pumpMessageMutex};
                TimeMs delay = waitTime - Now();
                if (delay < 1ms)
                    delay = 1ms;
                pumpMessageConditionVariable.wait_for(lock, delay);
            }
            PumpMessages();
        }
    }
    else
    {
        TimeMs delay = time - Now();
        if (delay < 1ms)
            delay = 1ms;
        std::this_thread::sleep_for(delay);
    }
}

void CoDispatcher::PumpMessages(std::chrono::milliseconds timeout)
{
    auto endTime = Now() + timeout;
    if (PumpMessages())
    {
        return;
    }

    if (IsForeground())
    {

        while (true)
        {
            auto now = Now();
            if (now >= endTime)
                throw CoTimedOutException();

            TimeMs waitTime = endTime;
            TimeMs timerDelay;
            if (GetNextTimer(&timerDelay))
            {
                if (timerDelay < waitTime)
                {
                    waitTime = timerDelay;
                }
            }
            {
                std::unique_lock lock{pumpMessageMutex};
                TimeMs delay = waitTime - now;
                if (delay < 1ms)
                    delay = 1ms;
                pumpMessageConditionVariable.wait_for(lock, delay);
            }
            if (PumpMessages())
            {
                return;
            }
        }
    }
    else
    {
        throw std::logic_error("Can't call this function on a background thread.");
    }
}

bool CoDispatcher::PumpMessages(bool waitForTimers)
{
    if (!IsForeground())
    {
        os::msleep(100);
        return true;
    }

    while (true)
    {
        bool messageProcessed = PumpMessages();
        if (messageProcessed)
        {
            return true;
        }
        if (waitForTimers)
        {
            PumpMessageWaitOne();
            return PumpMessages();
        }
        else
        {
            return false;
        }
    }
}
bool CoDispatcher::PumpMessages()
{
    if (!IsForeground())
    {
        os::msleep(100);
        return true;
    }

    bool processedAny = false;

    TimeMs now = Now();

    while (true)
    {
        bool processedMessage = false;

        if (PumpTimerMessages(now))
        {
            processedAny = true;
            processedMessage = false;
            while (PumpTimerMessages(now))
            {
                //
            }
        };

        {
            std::unique_lock<std::mutex> lock{schedulerMutex};
            while (!this->queue.empty())
            {
                processedAny = true;
                processedMessage = true;
                // pump posted messages.
                std::coroutine_handle<> t = queue.pop();
                lock.unlock();
                t.resume();
                lock.lock();
            }
        }
        if (!processedMessage)
        {
            pSchedulerPool->ScavengeDeadThreads();
            ScavengeTasks();

            return processedAny;
        }
    }
}

CoDispatcher::CoDispatcher()
{
    this->pForegroundDispatcher = this;
    this->pSchedulerPool = new CoTaskSchedulerPool(this);
}
CoDispatcher::CoDispatcher(CoDispatcher *pForegroundDispatcher, CoTaskSchedulerPool *pSchedulerPool)
{
    this->pForegroundDispatcher = pForegroundDispatcher;
    this->pSchedulerPool = pSchedulerPool;
}

CoDispatcher::~CoDispatcher()
{
    if (IsForeground())
    {
        if (this->pSchedulerPool)
        {
            delete this->pSchedulerPool;
            this->pSchedulerPool = nullptr;
        }
        Log().Debug("Dispatcher deleted.");
    }
    {
        std::lock_guard lock{gLogMutex}; // protect the shared_ptr.
        this->log = nullptr;
    }
}

static bool hasMainDispatcher = false;
CoDispatcher *CoDispatcher::CreateMainDispatcher()
{
    if (hasMainDispatcher)
    {
        throw std::logic_error("Operation performed on a non-dispatcher thread.");
    }
    hasMainDispatcher = true;

    return gForegroundDispatcher = new CoDispatcher();
}

void CoDispatcher::DestroyDispatcher()
{
    if (CoDispatcher::pInstance != nullptr)
    {
        CoDispatcher *p = pInstance;
        if (!p->IsForeground())
        {
            throw std::invalid_argument("Can only destroy the foreground dispatcher.");
        }
        gForegroundDispatcher = nullptr;
        pInstance = nullptr;
        delete p;
        hasMainDispatcher = false;
    }
}

bool CoDispatcher::IsDone() const
{
    if (!IsForeground())
    {
        return pForegroundDispatcher->IsDone();
    }
    if ((!coroutineTimerQueue.empty()) || (!queue.empty()) || (!functionTimerQueue.empty()))
    {
        return false;
    }
    return pSchedulerPool->IsDone();
}

size_t CoDispatcher::Instrumentation::GetThreadPoolSize()
{
    return CurrentDispatcher().pSchedulerPool->threads.size();
}
size_t CoDispatcher::Instrumentation::GetNumberOfDeadThreads()
{
    return CurrentDispatcher().pSchedulerPool->deadThreads.size();
}

void CoDispatcher::PumpMessageNotifyOne()
{
    {
        std::lock_guard lock{pumpMessageMutex};
        // cout << "notify" << endl;
        this->messagePosted = true;
        pumpMessageConditionVariable.notify_one();
    }
}

void CoDispatcher::PumpMessageWaitOne()
{
    while (true)
    {
        std::unique_lock lock{pumpMessageMutex};
        if (this->messagePosted)
        {
            this->messagePosted = false;
            return;
        }
        std::chrono::milliseconds nextTimer;
        if (this->GetNextTimer(&nextTimer))
        {
            auto delay = nextTimer - Now();
            if (delay.count() < 0)
            {
                return;
            }
            if (delay.count() == 0)
            {
                delay = 1ms;
            }
            pumpMessageConditionVariable.wait_for(lock, delay);
            return;
        }
        else
        {
            pumpMessageConditionVariable.wait_for(lock, 1000ms);
            return;
        }
    }
}

int scavengeTaskCounter = 0;
void CoDispatcher::ScavengeTasks()
{
    while (true)
    {
        bool removedOne = false;
        for (auto i = coroutineThreads.begin(); i != coroutineThreads.end(); /**/)
        {

            if (i->handle.done())
            {
                CoTask<> t = std::move(*i);
                i = coroutineThreads.erase(i);
                try
                {

                    t.GetResult();
                    removedOne = true;
                    break;
                }
                catch (const std::exception &e)
                {
                    Log().Error(SS("Coroutine Thread exited abnormally. (" << e.what() << ")"));
                    std::terminate();
                }
            }
            else
            {
                ++i;
            }
        }
        if (!removedOne)
            break;
    }
}

void CoDispatcher::StartThread(CoTask<> &&task)
{
    ScavengeTasks();
    coroutineThreads.insert(coroutineThreads.begin(), std::move(task));
}

void CoDispatcher::PostQuit()
{
    if (!IsForeground())
    {
        pForegroundDispatcher->PostQuit();
    }
    this->quit = true;
    this->PumpMessageNotifyOne();
}

void CoDispatcher::RemoveThreadDispatcher()
{
    if (pInstance != nullptr)
    {
        delete pInstance;
        pInstance = nullptr;
    }
}

void cotask::Terminate(const std::string &message)
{
    try
    {
        throw logic_error(message);
    }
    catch (const std::logic_error &)
    {
        std::terminate();
    }
}
