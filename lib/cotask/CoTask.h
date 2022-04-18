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
#include <coroutine>
#include <utility>
#include <queue>
#include <vector>
#include <exception>
#include <concepts>
#include <mutex>
#include <memory>
#include "Log.h"
#include "Fifo.h"
#include <functional>
#include <condition_variable>
#include <list>
#include "CoExceptions.h"

#ifdef __GNUC__
// ignoring the results of [[nodiscard]] CoTask<> fn() is a serious error! (not just a warning)
#pragma GCC diagnostic error "-Wunused-result"


#define P2P_GCC_VERSION() (__GNUC__*100 + __GNUC_MINOR__)

// coroutines generate suprious error "statement has no effect [-Werror=unused-value] in GCC 10.2
// (Fixed  in GCC 10.3.0)
#if P2P_GCC_VERSION() < 1003

#   pragma GCC diagnostic ignored "-Wunused-value"
#endif

#endif



namespace cotask
{

    // forward declarations.
    template <typename... Dummy>
    struct CoTask;
    template <typename T>
    struct CoTask<T>;
    template <>
    struct CoTask<>;

    constexpr std::chrono::milliseconds NO_TIMEOUT = std::chrono::milliseconds(-1);

    template <typename T, typename RETURN_TYPE>
    concept Awaitable = requires(T a, std::coroutine_handle<> h)
    {
        {
            a.await_ready()
        }
        ->std::convertible_to<bool>;
        {a.await_suspend(h)};
        {
            a.await_resume()
        }
        ->std::convertible_to<RETURN_TYPE>;
    };

    // Forward declarations.
    class CoTaskSchedulerPool;
    class CoTaskSchedulerThread;

    // CoDispatcher
    class CoDispatcher
    {
    public:
        using TimeMs = std::chrono::milliseconds;

    private:
        static CoDispatcher *CreateMainDispatcher();
        static CoDispatcher *gForegroundDispatcher;

        CoDispatcher();
        CoDispatcher(CoDispatcher *pForegroundDispatcher, CoTaskSchedulerPool *pSchedulerPool);
        ~CoDispatcher();

    public:
        static CoDispatcher &ForegroundDispatcher();
        static CoDispatcher &CurrentDispatcher();
        static void DestroyDispatcher();

        CoDispatcher *GetForegroundDispatcher() { return pForegroundDispatcher; }

        static TimeMs Now();

        void Post(std::coroutine_handle<> handle);
        void PostBackground(std::coroutine_handle<> handle);
        void PostDelayed(TimeMs delay, const std::coroutine_handle<> &handle);
        uint64_t PostDelayedFunction(TimeMs delay, std::function<void(void)> fn);
        bool CancelDelayedFunction(uint64_t timerHandle);

        bool GetNextTimer(CoDispatcher::TimeMs *pResult) const;
        void SleepFor(TimeMs delay);
        void SleepUntil(TimeMs time);

        bool IsDone() const;

        bool PumpMessages();
        bool PumpMessages(bool waitForTimers);

        /**
         * @brief Pump messages.
         * 
         * Pump at least one message, sleeping if neccessary.
         * 
         * @param timeout how long to wait for a message to pump.
         * @throws CoTimedOutException if the timeout occurs before any messages are pumped.
         */
        void PumpMessages(std::chrono::milliseconds timeout);

        void PumpUntilIdle();

        void MessageLoop();
        void MessageLoop(CoTask<> &&threadMain);

        void PostQuit();

        void SetThreadPoolSize(size_t threads);

        bool IsForeground() const
        {
            return this == pForegroundDispatcher;
        }

        ILog &Log() const { return *(log.get()); }

        void SetLog(const std::shared_ptr<ILog> &log)
        {
            std::lock_guard lock{gLogMutex}; // protect the shared ptr instance.
            this->log = log;
        }

    public:
        bool PumpTimerMessages(TimeMs time);
        // Test Instrumetnation
        class Instrumentation
        {
        public:
            static size_t GetThreadPoolSize();
            static size_t GetNumberOfDeadThreads();
        };

        void StartThread(CoTask<> &&task);

    private:
        std::list<CoTask<>> coroutineThreads;
        void ScavengeTasks();
        bool inMessageLoop = false;
        bool quit = false;

        bool messagePosted = false;
        void PumpMessageNotifyOne();
        void PumpMessageWaitOne();
        uint64_t nextTimerHandle = 0;
        static std::mutex gLogMutex;
        std::shared_ptr<ILog> log = std::make_shared<ConsoleLog>();
        friend class CoTaskSchedulerPool;
        friend class CoTaskSchedulerThread;
        static void RemoveThreadDispatcher();

        std::mutex pumpMessageMutex;
        std::condition_variable pumpMessageConditionVariable;

        std::mutex schedulerMutex;
        static std::mutex creationMutex;

        static thread_local CoDispatcher *pInstance;
        CoDispatcher *pForegroundDispatcher;
        CoTaskSchedulerPool *pSchedulerPool;

        struct CoroutineTimerEntry
        {
            TimeMs time;
            std::coroutine_handle<> handle;
        };
        struct TimerFunctionEntry
        {
            TimeMs time;
            std::function<void(void)> fn;
            uint64_t timerHandle;
        };

        struct TeLess
        {
            bool
            operator()(const struct CoroutineTimerEntry &__x, const struct CoroutineTimerEntry &__y) const
            {
                return __x.time > __y.time;
            }
        };

        std::priority_queue<CoroutineTimerEntry, std::vector<CoroutineTimerEntry>, TeLess> coroutineTimerQueue;

        struct TeFnLess
        {
            bool
            operator()(const struct TimerFunctionEntry &__x, const struct TimerFunctionEntry &__y) const
            {
                return __x.time > __y.time;
            }
        };
        std::mutex delayedFunctionMutex;

        // Order N^+N insertion+delete. Priority queue is N log(N) insert, but still N^2+N delete.
        // The priority isn't really faster, and rebalancing after erase() is awkwardly difficult.

        std::list<TimerFunctionEntry> functionTimerQueue;

        static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;
        Fifo<std::coroutine_handle<>> queue;
    };

    template <typename... Dummy>
    struct CoTask;
    template <typename T>
    struct CoTask<T>;

    template <typename T>
    struct [[nodiscard("Are you missing a co_await?")]] CoTask<T>
    {

        // The return type of a coroutine must contain a nested struct or type alias called `promise_type`
        struct promise_type

        {
            ~promise_type()
            {
            }
            /********************DATA ************************/
            // Keep a coroutine handle referring to the parent coroutine if any. That is, if we
            // co_await a coroutine within another coroutine, this handle will be used to continue
            // working from where we left off.
            std::coroutine_handle<> precursor;

            // Place to hold the results produced by the coroutine
            T data;

            // Invoked when we first enter a coroutine. We initialize the precursor handle
            // with a resume point from where the task is ultimately suspended

            // This handle is assigned to when the coroutine itself is suspended (see await_suspend)
            std::coroutine_handle<promise_type> handle;

            CoTask get_return_object() noexcept
            {
                return {std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            // When the caller enters the coroutine, we have the option to suspend immediately.
            // Let's choose not to do that here
            std::suspend_never initial_suspend() const noexcept { return {}; }

            std::exception_ptr unhandledException;
            void unhandled_exception()
            {
                unhandledException = std::current_exception();
            }

            // The coroutine is about to complete (via co_return or reaching the end of the coroutine body).
            // The awaiter returned here defines what happens next
            auto final_suspend() const noexcept
            {
                struct awaiter
                {
                    // Return false here to return control to the thread's event loop. Remember that we're
                    // running on some async thread at this point.
                    bool await_ready() const noexcept { return false; }

                    void await_resume() const noexcept {}

                    // Returning a coroutine handle here resumes the coroutine it refers to (needed for
                    // continuation handling). If we wanted, we could instead enqueue that coroutine handle
                    // instead of immediately resuming it by enqueuing it and returning void.
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h)
                    {
                        auto precursor = h.promise().precursor;
                        if (precursor)
                        {
                            return precursor;
                        }
                        return std::noop_coroutine();
                    }
                };
                return awaiter{};
            }

            // When the coroutine co_returns a value, this method is used to publish the result
            void return_value(T &&value) noexcept
            {
                data = std::move(value);
            }
            void return_value(const T &value) noexcept
            {
                data = std::move(value);
            }
        };

        // The following methods make our task type conform to the awaitable concept, so we can
        // co_await for a task to complete

        bool await_ready() const noexcept
        {
            // No need to suspend if this task has no outstanding work
            return handle.done();
        }

        T await_resume() const
        {
            auto &promise = handle.promise();
            if (promise.unhandledException)
            {
                std::rethrow_exception(promise.unhandledException);
            }
            // The returned value here is what `co_await our_task` evaluates to
            return std::move(promise.data);
        }

        void await_suspend(std::coroutine_handle<> coroutine) const noexcept
        {
            // The coroutine itself is being suspended (async work can beget other async work)
            // Record the argument as the continuation point when this is resumed later. See
            // the final_suspend awaiter on the promise_type above for where this gets used
            handle.promise().precursor = coroutine;
        }
        // This handle is assigned to when the coroutine itself is suspended (see await_suspend above)
        std::coroutine_handle<promise_type> handle = nullptr;

        CoTask(std::coroutine_handle<promise_type> handle)
            : handle(handle)
        {
        }

        CoTask(CoTask<T> &&other)
            : handle(std::exchange(other.handle, nullptr))
        {
        }
        CoTask<T> &operator=(CoTask<T> &&other)
        {
            if (this != &other)
            {
                if (this->handle != nullptr)
                {
                    this->handle.destroy();
                }
                this->handle = std::exchange(other.handle, nullptr);
            }
            return *this;
        }
        ~CoTask()
        {
            if (handle != nullptr)
            {
                handle.destroy();
            }
        }

        T GetResult();
    };

    inline CoDispatcher &Dispatcher() { return CoDispatcher::CurrentDispatcher(); }

    template <>
    struct [[nodiscard("Are you missing a co_await?")]] CoTask<>
    {
        // The return type of a coroutine must contain a nested struct or type alias called `promise_type`
        struct promise_type
        {
            ~promise_type()
            {
            }
            // Keep a coroutine handle referring to the parent coroutine if any. That is, if we
            // co_await a coroutine within another coroutine, this handle will be used to continue
            // working from where we left off.
            std::coroutine_handle<> precursor;

            // Invoked when we first enter a coroutine. We initialize the precursor handle
            // with a resume point from where the task is ultimately suspended
            std::coroutine_handle<promise_type> get_return_object() noexcept
            {
                return std::coroutine_handle<promise_type>::from_promise(*this);
            }

            // When the caller enters the coroutine, we have the option to suspend immediately.
            // Let's choose not to do that here
            std::suspend_never initial_suspend() const noexcept { return {}; }

            std::exception_ptr unhandledException;
            void unhandled_exception()
            {
                unhandledException = std::current_exception();
            }

            // The coroutine is about to complete (via co_return or reaching the end of the coroutine body).
            // The awaiter returned here defines what happens next
            auto final_suspend() const noexcept
            {
                struct awaiter
                {
                    // Return false here to return control to the thread's event loop. Remember that we're
                    // running on some async thread at this point.
                    bool await_ready() const noexcept { return false; }

                    void await_resume() const noexcept {}

                    // Returning a coroutine handle here resumes the coroutine it refers to (needed for
                    // continuation handling). If we wanted, we could instead enqueue that coroutine handle
                    // instead of immediately resuming it by enqueuing it and returning void.
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept
                    {
                        auto precursor = h.promise().precursor;
                        if (precursor)
                        {
                            return precursor;
                        }
                        return std::noop_coroutine();
                    }
                };
                return awaiter{};
            }

            // When the coroutine co_returns a value, this method is used to publish the result
            void return_void() noexcept
            {
            }
        };

        // The following methods make our task type conform to the awaitable concept, so we can
        // co_await for a task to complete

        bool await_ready() const noexcept
        {
            // No need to suspend if this task has no outstanding wiork
            return handle.done();
        }

        void await_resume() const
        {
            auto &promise = handle.promise();
            if (promise.unhandledException)
            {
                std::rethrow_exception(promise.unhandledException);
            }
        }

        void await_suspend(std::coroutine_handle<> coroutine) const noexcept
        {
            // The coroutine itself is being suspended (async work can beget other async work)
            // Record the argument as the continuation point when this is resumed later. See
            // the final_suspend awaiter on the promise_type above for where this gets used
            handle.promise().precursor = coroutine;
        }

        void GetResult()
        {
            while (!handle.done())
            {
                CoDispatcher::CurrentDispatcher().PumpMessages(true);
            }
            try
            {
                await_resume();
            }
            catch (const std::exception &e)
            {
                try {
                    handle.destroy();
                    handle = nullptr;
                } catch (const std::exception &e2)
                {
                    Dispatcher().Log().Error(e.what());
                    Dispatcher().Log().Error(std::string("Exception while deleting coroutine. ") + e2.what() );
                    std::terminate();
                }

                throw;
            }
            try {
                handle.destroy();
                handle = nullptr;
            }
            catch(const std::exception& e)
            {
                Dispatcher().Log().Error(e.what());
                std::terminate();
            }
                
            return;
        }

        // This handle is assigned to when the coroutine itself is suspended (see await_suspend above)
        std::coroutine_handle<promise_type> handle;

        CoTask()
        :handle(nullptr) {
            
        }

        // To satisfy VS code. Unsure if this is correct in newer C++20 standards.
        CoTask(std::coroutine_handle<promise_type> handle)
        {
            this->handle = handle;
        }

        CoTask(CoTask<> &&other)
            : handle(std::exchange(other.handle, nullptr))
        {
        }
        CoTask<> &operator=(CoTask<> &&other)
        {
            if (this->handle != nullptr)
            {
                this->handle.destroy();
            }
            this->handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        ~CoTask()
        {
            if (handle != nullptr)
            {
                handle.destroy();
            }
        }
    };

    //***************************************************************
    inline auto CoDelay(CoDispatcher::TimeMs delayMs) noexcept
    {
        struct awaiter
        {
            ~awaiter() {

            }
            CoDispatcher::TimeMs delayMs;
            awaiter(CoDispatcher::TimeMs delayMs)
            {
                this->delayMs = delayMs;
            }
            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> coroutine) noexcept
            {
                    CoDispatcher::CurrentDispatcher().PostDelayed(delayMs, coroutine);
            }

            void await_resume() const noexcept
            {
            }
        };
        return awaiter{delayMs};
    }

    inline auto CoForeground() noexcept
    {
        struct awaiter
        {

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> coroutine) noexcept
            {
                CoDispatcher::CurrentDispatcher().Post(coroutine);
            }

            void await_resume() const noexcept
            {
            }
        };
        return awaiter{};
    }
    inline auto CoBackground() noexcept
    {
        struct awaiter
        {
            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> coroutine) noexcept
            {
                CoDispatcher::CurrentDispatcher().PostBackground(coroutine);
            }

            void await_resume() const noexcept
            {
            }
        };
        return awaiter{};
    }

    /**
     * @brief Terminate the current process with an error message.
     * 
     * Used to indicate that something went irrecoverably wrong.
     * 
     * @param message The message to display when terminating.
     */
    extern void Terminate(const std::string & message);

    /**************************/

    inline CoDispatcher& CoDispatcher::ForegroundDispatcher() 
    {
        return *gForegroundDispatcher;
    }
    inline CoDispatcher &CoDispatcher::CurrentDispatcher()
    {
        CoDispatcher *pResult = pInstance;
        if (pResult == nullptr)
            pResult = pInstance = CoDispatcher::CreateMainDispatcher();
        return *pResult;
    }

    template <typename T>
    T CoTask<T>::GetResult()
    {
        while (!handle.done())
        {
            CoDispatcher::CurrentDispatcher().PumpMessages(true);
        }
        try
        {
            T value = await_resume();
            handle.destroy();
            handle = nullptr;
            return value;
        }
        catch (std::exception &e)
        {
            handle.destroy();
            handle = nullptr;
            throw;
        }
    }

} // namespace