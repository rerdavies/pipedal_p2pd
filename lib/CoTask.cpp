#include "p2psession/CoTask.h"
#include <chrono>

#include "CoTaskSchedulerPool.h"

using namespace p2psession;

using Clock = std::chrono::steady_clock;

thread_local CoDispatcher *CoDispatcher::pInstance;

CoDispatcher::TimeMs CoDispatcher::Now()
{

    auto now = Clock::now();
    auto duration = now.time_since_epoch();

    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void CoDispatcher::PostDelayed(TimeMs delay, const std::coroutine_handle<> &handle)
{
    if (!IsForeground())
    {
        pForegroundDispatcher->PostDelayed(delay,handle);
    } else
    {
        std::lock_guard lock{this->schedulerMutex};
        TimerEntry entry{Now() + delay, handle};
        timerQueue.push(std::move(entry));
    }
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
        std::lock_guard lock{schedulerMutex};
        queue.push(handle);
    }
}

void CoDispatcher::PumpUntilDone() {
    while (!IsDone())
    {
        PumpMessages();
    }
}

void CoDispatcher::SetThreadPoolSize(size_t size)
{
    if (!IsForeground())
    {
        throw std::invalid_argument("Not allowed on a background thread.");
    }
    pSchedulerPool->Resize(size);
}
bool CoDispatcher::PumpMessages()
{
    if (!IsForeground())
    {
        throw std::invalid_argument("Can't pump on a background thread.");
    }
    pSchedulerPool->ScavengeDeadThreads();
    schedulerMutex.lock();

    while (true)
    {
        bool processedMessage = false;

        // pump timer messages.
        TimeMs now = Now();
        while (!timerQueue.empty() && timerQueue.top().time < now)
        {
            auto handle = timerQueue.top().handle;
            timerQueue.pop();
            schedulerMutex.unlock();
            handle.resume();
            schedulerMutex.lock();
            processedMessage = true;
        }

        while (!this->queue.empty())
        {
            processedMessage = true;

            // pump posted messages.
            std::coroutine_handle<> t = queue.pop();
            schedulerMutex.unlock();
            t.resume();
            schedulerMutex.lock();
        }
        if (!processedMessage)
        {
            schedulerMutex.unlock();
            return processedMessage;
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

void CoDispatcher::DestroyDispatcher()
{
    if (CoDispatcher::pInstance != nullptr)
    {
        CoDispatcher*p = pInstance;
        if (!p->IsForeground())
        {
            throw std::invalid_argument("Can only destroy the foreground dispatcher.");
        }
        pInstance = nullptr;
        delete p;
        
    }
}

bool CoDispatcher::IsDone() const
{
    if (!IsForeground())
    {
        return pForegroundDispatcher->IsDone();
    }
    if ((!timerQueue.empty()) || (!queue.empty()))
    {
        return false;
    }
    return pSchedulerPool->IsDone();
}


size_t CoDispatcher::Instrumentation::GetThreadPoolSize() {
    return CurrentDispatcher().pSchedulerPool->threads.size();
} 
size_t CoDispatcher::Instrumentation::GetNumberOfDeadThreads() {
    return CurrentDispatcher().pSchedulerPool->deadThreads.size();
} 
