#pragma once
#include "p2psession/CoTask.h"
#include <thread>
#include <exception>
#include <coroutine>
#include <condition_variable>

namespace p2psession
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



        std::mutex schedulerMutex;
        std::condition_variable readyToRun;

        void OnThreadTerminated(CoTaskSchedulerThread*thread);

        std::mutex threadTerminatedMutex;
        std::condition_variable threadTerminated;
        Fifo<std::coroutine_handle<>> handleQueue;
    };

} // namespace