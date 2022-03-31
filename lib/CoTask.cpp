#include "p2psession/CoTask.h"
#include <chrono>
#include "ss.h"
#include "CoTaskSchedulerPool.h"

using namespace p2psession;
using namespace std;

using Clock = std::chrono::steady_clock;

thread_local CoDispatcher *CoDispatcher::pInstance;

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
    while (!IsDone())
    {
        PumpMessages();
    }
}

void CoDispatcher::MessageLoop(CoTask<> threadMain)
{
    StartThread(threadMain);
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
                functionTimerQueue.pop_front();
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
            if (coroutineTimerQueue.top().time.count() < time.count())
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
            if (functionTimerQueue.begin()->time.count() <= coroutineTimerQueue.top().time.count())
            {
                auto fn = functionTimerQueue.begin()->fn;
                functionTimerQueue.pop_front();
                lock.unlock();
                fn();
                return true;
            }
            else
            {
                auto handle = coroutineTimerQueue.top().handle;
                coroutineTimerQueue.pop();
                lock.unlock();
                handle.resume();
                return true;
            }
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

bool CoDispatcher::PumpMessages(bool waitForTimers)
{
    bool messageProcessed = PumpMessages();
    if (messageProcessed)
    {
        return true;
    }
    std::chrono::milliseconds nextTimer;
    {
        if (quit)
        {
            return messageProcessed;
        }
        std::unique_lock lock{pumpMessageMutex};
        if (GetNextTimer(&nextTimer))
        {
            {
                TimeMs delay = nextTimer - Now();
                if (delay < 1ms)
                    delay = 1ms;
                this->pumpMessageConditionVariable.wait_for(lock, nextTimer - Now());
            }
            lock.unlock();
            return PumpMessages();
        }
    }
    return false;
}
bool CoDispatcher::PumpMessages()
{
    if (!IsForeground())
    {
        throw std::invalid_argument("Can't pump on a background thread.");
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
        delete this->pSchedulerPool;
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
    return new CoDispatcher();
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
    std::lock_guard lock{pumpMessageMutex};
    pumpMessageConditionVariable.notify_one();
}

void CoDispatcher::PumpMessageWaitOne()
{
    std::unique_lock lock{pumpMessageMutex};
    pumpMessageConditionVariable.wait_for(lock, 1000ms);
}

int scavengeTaskCounter = 0;
void CoDispatcher::ScavengeTasks()
{
    for (auto i = coroutineThreads.begin(); i != coroutineThreads.end(); /**/)
    {
    
        if (i->handle.done())
        {
            try {

                i->GetResult();
            } catch (std::exception e)
            {
                Log().Error(SS("Coroutine Thread exited abnormally. (" << e.what() << ")"));
            }
            i = coroutineThreads.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

void CoDispatcher::StartThread(CoTask<> task)
{
    ScavengeTasks();
    coroutineThreads.insert(coroutineThreads.begin(), task);
}

void CoDispatcher::Quit()
{
    if (!IsForeground())
    {
        pForegroundDispatcher->Quit();
    }   
    this->quit = true;
    this->PumpMessageNotifyOne();
}
