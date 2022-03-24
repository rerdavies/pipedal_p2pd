
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

namespace p2psession
{

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
        using TimeMs = int64_t;

    private:
        CoDispatcher();
        CoDispatcher(CoDispatcher *pForegroundDispatcher, CoTaskSchedulerPool *pSchedulerPool);
        ~CoDispatcher();

    public:
        static CoDispatcher &CurrentDispatcher();

        static void DestroyDispatcher();

        static TimeMs Now();

        void Post(std::coroutine_handle<> handle);
        void PostBackground(std::coroutine_handle<> handle);
        void PostDelayed(TimeMs delay, const std::coroutine_handle<> &handle);

        bool IsDone() const;

        bool PumpMessages();

        void PumpUntilDone();

        void SetThreadPoolSize(size_t threads);

        bool IsForeground() const
        {
            return this == pForegroundDispatcher;
        }

        ILog &Log() { return *(log.get()); }

        void SetLog(const std::shared_ptr<ILog> &log)
        {
            this->log = log;
        }

    public:
        // Test Instrumetnation
        class Instrumentation
        {
        public:
            static size_t GetThreadPoolSize();
            static size_t GetNumberOfDeadThreads();
        };

    private:
        std::shared_ptr<ILog> log = std::make_shared<ConsoleLog>();
        friend class CoTaskSchedulerPool;
        friend class CoTaskSchedulerThread;

        std::mutex schedulerMutex;
        static std::mutex creationMutex;

        static thread_local CoDispatcher *pInstance;
        CoDispatcher *pForegroundDispatcher;
        CoTaskSchedulerPool *pSchedulerPool;

        struct TimerEntry
        {
            TimeMs time;
            std::coroutine_handle<> handle;
        };

        struct TeLess
        {
            bool
            operator()(const struct TimerEntry &__x, const struct TimerEntry &__y) const
            {
                return __x.time > __y.time;
            }
        };
        std::priority_queue<TimerEntry, std::vector<TimerEntry>, TeLess> timerQueue;
        static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;
        Fifo<std::coroutine_handle<>> queue;
    };

    template <typename... Dummy>
    struct Task;

    template <typename T>
    struct Task<T>
    {
        // The return type of a coroutine must contain a nested struct or type alias called `promise_type`
        struct promise_type
        {
            // Keep a coroutine handle referring to the parent coroutine if any. That is, if we
            // co_await a coroutine within another coroutine, this handle will be used to continue
            // working from where we left off.
            std::coroutine_handle<> precursor;

            // Place to hold the results produced by the coroutine
            T data;

            // Invoked when we first enter a coroutine. We initialize the precursor handle
            // with a resume point from where the task is ultimately suspended
            Task get_return_object() noexcept
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
            auto final_suspend() const
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
            auto promise = handle.promise();
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



        T GetResult();

        // This handle is assigned to when the coroutine itself is suspended (see await_suspend above)
        std::coroutine_handle<promise_type> handle;
    };

    template <>
    struct Task<>
    {
        // The return type of a coroutine must contain a nested struct or type alias called `promise_type`
        struct promise_type
        {
            // Keep a coroutine handle referring to the parent coroutine if any. That is, if we
            // co_await a coroutine within another coroutine, this handle will be used to continue
            // working from where we left off.
            std::coroutine_handle<> precursor;

            // Invoked when we first enter a coroutine. We initialize the precursor handle
            // with a resume point from where the task is ultimately suspended
            Task<> get_return_object() noexcept
            {
                return Task<>{std::coroutine_handle<promise_type>::from_promise(*this)};
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
            auto final_suspend() const 
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
            void return_void() noexcept
            {
            }
        };

        // The following methods make our task type conform to the awaitable concept, so we can
        // co_await for a task to complete

        bool await_ready() const noexcept
        {
            // No need to suspend if this task has no outstanding work
            return handle.done();
        }

        void await_resume() const noexcept
        {
            auto promise = handle.promise();
            if (promise.unhandledException)
            {
                try {
                    std::rethrow_exception(promise.unhandledException);
                } catch (std::exception &e)
                {
                    std::cout << "Got it " << std::endl;
                }
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
                CoDispatcher::CurrentDispatcher().PumpMessages();
            }
            try
            {
                await_resume();
                handle.destroy();
                return;
            }
            catch (std::exception &e)
            {
                handle.destroy();
                throw;
            }
        }

        // This handle is assigned to when the coroutine itself is suspended (see await_suspend above)
        std::coroutine_handle<promise_type> handle;
    };
    //***************************************************************
    inline auto CoDelay(CoDispatcher::TimeMs delayMs) noexcept
    {
        struct awaiter
        {
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

    /**************************/

    inline CoDispatcher &CoDispatcher::CurrentDispatcher()
    {
        CoDispatcher *pResult = pInstance;
        if (pResult == nullptr)
            pResult = pInstance = new CoDispatcher();
        return *pResult;
    }

    template <typename T>
    T Task<T>::GetResult()
    {
        while (!handle.done())
        {
            CoDispatcher::CurrentDispatcher().PumpMessages();
        }
        try
        {
            T value = await_resume();
            handle.destroy();
            return value;
        }
        catch (std::exception &e)
        {
            handle.destroy();
            throw;
        }
    }



} // namespace