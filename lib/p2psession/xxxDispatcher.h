#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <vector>
#include "P2pException.h"
#include <functional>
#include <chrono>
#include <queue>

namespace p2psession
{

    extern int gAllocatedPromises;

    using DeleteListener = std::function<void()>;

    template <typename... Dummy>
    class AwaitablePromise;

    struct PromiseBase
    {
        PromiseBase()
        {
            ++gAllocatedPromises; // for debug tracking.
        }
        virtual ~PromiseBase()
        {
            --gAllocatedPromises;
            for (auto &entry : deleteListeners)
            {
                entry.onDelete();
            }
        }
        virtual void cancel() = 0;
        virtual void timeOut() = 0;

    private:
        struct DeleteListenerEntry
        {
            DeleteListener onDelete;
            uint64_t handle;
        };

    public:
        using DeleteListener = std::function<void(void)>;
        uint64_t addDeleteListener(const DeleteListener &onDelete)
        {
            uint64_t handle = nextHandle++;
            DeleteListenerEntry entry = {onDelete, handle};
            deleteListeners.push_back(std::move(entry));
            return handle;
        }
        void removeDeleteListener(uint64_t handle)
        {
            for (auto iter = deleteListeners.begin(); iter != deleteListeners.end(); ++iter)
            {
                if (iter->handle == handle)
                {
                    deleteListeners.erase(iter);
                    return;
                }
            }
        }

    private:
        uint64_t nextHandle = 0x10;
        std::vector<DeleteListenerEntry> deleteListeners;
    };

    template <typename TASK, typename RETURN_TYPE>
    struct AwaitablePromise<TASK, RETURN_TYPE> : public PromiseBase
    {
        using promise_type = AwaitablePromise<TASK, RETURN_TYPE>;

        std::exception_ptr returnException;
        RETURN_TYPE value;
        bool ready = false;

        AwaitablePromise()
        {
        }
        virtual ~AwaitablePromise()
        {
        }
        uint64_t nextHandle = 0x1;

        auto getHandle()
        {
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }

        TASK get_return_object()
        {
            return TASK{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(const RETURN_TYPE &value)
        {
            this->value = value;
            signal();
        }
        void return_value(RETURN_TYPE &&value)
        {
            this->value = std::move(value);
            signal();
        }
        RETURN_TYPE get_value() const
        {
            if (returnException)
            {
                std::rethrow_exception(returnException);
            }
            return value;
        }

        void signal()
        {
            this->ready = true;
        }
        virtual void cancel()
        {
            try
            {
                throw P2pCancelledException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }
        virtual void timeOut()
        {
            try
            {
                throw P2pTimeoutException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }
        void unhandled_exception()
        {
            returnException = std::current_exception();
            signal();
        }
        bool await_suspend(std::coroutine_handle<> handle) noexcept
        {
            return false;
        }

        bool await_ready()
        {
            return true;
        }
    };

    template <typename... Dummy>
    class Awaitable;

    template <typename T>
    class Awaitable<T> : public std::coroutine_handle<AwaitablePromise<Awaitable<T>, T>>
    {
    public:
        using promise_type = struct AwaitablePromise<Awaitable<T>, T>;
        using handle_type = std::coroutine_handle<promise_type>;

        Awaitable() {}
        ~Awaitable() {}

        std::coroutine_handle<promise_type> h_; // B.1: member variable

        Awaitable(std::coroutine_handle<promise_type> h) : h_{h}
        {
            this->_M_fr_ptr = h.address();
        }

        promise_type &promise() { return h_.promise(); }

        void await_suspend(std::coroutine_handle<> handle)
        {
            h_.promise().await_suspend(handle);
        }

        T await_resume()
        {
            try
            {
                T value = promise().get_value();
                this->destroy();
                return value;
            }
            catch (...)
            {
                this->destroy();
                throw;
            }
        }
        bool await_ready()
        {
            return promise().await_ready();
        }
        void return_value(const T &value)
        {
            promise().return_value(value);
        }
        void return_value(T &&value)
        {
            promise().return_value(std::move(value));
        }

        void cancel()
        {
            promise().cancel();
        }
        void timeOut()
        {
            promise().timeOut();
        }
        uint64_t addDeleteListener(DeleteListener listener)
        {
            return promise().addDeleteListener(listener);
        }
        void removeDeleteListener(uint64_t handle)
        {
            promise().removeDeleteListener(handle);
        }
    };

    template <typename T>
    class EventSource
    {
    public:
        void Fire(T value)
        {
            std::vector<Awaitable<T>> callbacks;
            listeners.swap(callbacks);

            for (auto &callback : callbacks)
            {
                callback.return_value(value);
            }
        }
        void Cancel()
        {
            std::vector<Awaitable<T>> callbacks;
            listeners.swap(callbacks);

            for (auto &callback : callbacks)
            {
                callback.cancel();
            }
        }
        static T defaultT;
        Awaitable<T> Wait()
        {
            Awaitable<T> result = WaitCoroutine();
            listeners.push_back(result);
            return result;
        }

    private:
        Awaitable<T> WaitCoroutine()
        {
            co_return defaultT;
        }

        std::vector<Awaitable<T>> listeners;
    };

    template <typename T>
    T EventSource<T>::defaultT;

    template <typename T>
    struct Generator
    {
        struct promise_type;
        using handle = std::coroutine_handle<promise_type>;
        struct promise_type
        {
            T current_value;
            auto get_return_object() { return Generator{handle::from_promise(*this)}; }
            std::suspend_always initial_suspend() noexcept { return std::suspend_always{}; }
            std::suspend_always final_suspend() noexcept { return std::suspend_always{}; }
            void unhandled_exception() { std::terminate(); }

            void return_value(T value) { this->current_value = value; }
            auto yield_value(T &&value)
            {
                current_value = std::forward<T>(value);
                return std::suspend_always{};
            }
            auto yield_value(const T &value)
            {
                current_value = value;
                return std::suspend_always{};
            }
        };

        bool move_next() { return coro ? (coro.resume(), !coro.done()) : false; }
        T current_value() { return coro.promise().current_value; }

        Generator(Generator const &) = delete;
        Generator(Generator &&rhs) : coro(rhs.coro) { rhs.coro = nullptr; }
        ~Generator()
        {
            if (coro)
                coro.destroy();
        }

    private:
        Generator(handle h) : coro(h) {}
        handle coro;
    };

    template <typename... Dummy>
    struct TaskPromise;

    template <typename TASK, typename T>
    struct TaskPromise<TASK, T> : public PromiseBase
    {
        using promise_type = TaskPromise<TASK, T>;
        T returnValue{};
        std::exception_ptr returnException;

        TaskPromise()
        {
        }
        ~TaskPromise()
        {
        }

        virtual void cancel()
        {
            try
            {
                throw P2pCancelledException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }
        virtual void timeOut()
        {
            try
            {
                throw P2pTimeoutException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }

        auto getHandle()
        {
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }

        TASK get_return_object()
        {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        bool await_suspend(std::coroutine_handle<> handle) noexcept;

        void return_value(const T &value)
        {
            returnValue = value;
            m_complete = true;
        }

        auto yield_value(T &&value)
        {
            returnValue = std::forward<T>(value);
            return std::suspend_always{};
        }
        auto yield_value(const T &value)
        {
            returnValue = value;
            return std::suspend_always{};
        }

        void return_value(T &&value)
        {
            returnValue = std::move(value);
            m_complete = true;
        }

        T get_value() const
        {
            if (returnException)
            {
                std::rethrow_exception(returnException);
            }
            return returnValue;
        }

        void unhandled_exception()
        {
            returnException = std::current_exception();
        }

        bool await_complete() { return m_complete; }
        bool await_ready() { return true; }


    private:
        bool m_complete = false;
    };

    template <typename TASK>
    struct TaskPromise<TASK> : public PromiseBase
    {
        using promise_type = TaskPromise<TASK>;

        std::exception_ptr returnException;

        TaskPromise()
        {
        }
        virtual ~TaskPromise()
        {
        }

        virtual void cancel()
        {
            try
            {
                throw P2pCancelledException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }
        virtual void timeOut()
        {
            try
            {
                throw P2pTimeoutException();
            }
            catch (...)
            {
                unhandled_exception();
            }
        }

        auto getHandle()
        {
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }

        TASK get_return_object()
        {
            return TASK{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void()
        {
            m_complete = true;
        }

        void unhandled_exception()
        {
            returnException = std::current_exception();
        }
        bool await_suspend(std::coroutine_handle<> handle) noexcept;

        bool await_complete() const { return m_complete; }
        bool await_ready() { return true; }

        void await_resume()
        {
            getHandle().resume();
        }


        bool complete() const { return m_complete; }

    private:
        bool m_complete = false;
    };

    template <typename... Dummy>
    class Task;

    template <typename T>
    struct Task<T> : public std::coroutine_handle<TaskPromise<Task<T>, T>>
    {
        using promise_type = struct TaskPromise<Task<T>, T>;
        using handle_type = std::coroutine_handle<promise_type>;

        Task() {}

        ~Task() {}

        std::coroutine_handle<promise_type> h_; // B.1: member variable
        Task(std::coroutine_handle<promise_type> h) : h_{h}
        {
            this->_M_fr_ptr = h.address();
        }

        promise_type &promise() { return h_.promise(); }

        bool await_suspend(std::coroutine_handle<> handle)
        {
            return h_.promise().await_suspend(handle);
        }

        T await_resume()
        {
            this->resume();
            return promise().get_value();
        }
        bool await_ready()
        {
            return promise().await_ready();
        }

        T execute()
        {
            while (!this->done())
            {
                await_resume();
            }
            T result = this->promise().get_value();
            return result;
        }
        void cancel()
        {
            promise().cancel();
        }
        void timeOut()
        {
            promise().timeOut();
        }
        uint64_t addDeleteListener(DeleteListener listener)
        {
            return promise().addDeleteListener(listener);
        }
        void removeDeleteListener(uint64_t handle)
        {
            promise().removeDeleteListener(handle);
        }
    };

    template <>
    struct Task<> : public std::coroutine_handle<TaskPromise<Task<>>>
    {
        using promise_type = struct TaskPromise<Task<>>;
        using handle_type = std::coroutine_handle<promise_type>;

        Task() {}

        std::coroutine_handle<promise_type> h_; // B.1: member variable
        Task(std::coroutine_handle<promise_type> h) : handle_type(h),h_{h}
        {
        }

        auto get_awaiter() { return h_.promise(); }

        bool await_ready()
        {
            return get_awaiter().await_ready();
        }

        bool await_suspend(std::coroutine_handle<> handle)
        {
            return get_awaiter().await_suspend(handle);
        }

        void await_resume()
        {
            this->resume();
        }

        void cancel()
        {
            promise().cancel();
        }
        void timeOut()
        {
            promise().timeOut();
        }
        uint64_t addDeleteListener(DeleteListener listener)
        {
            return promise().addDeleteListener(listener);
        }
        void removeDeleteListener(uint64_t handle)
        {
            promise().removeDeleteListener(handle);
        }
    };

    class Dispatcher
    {
    private:
        Dispatcher();
        ~Dispatcher();

    public:
        static Dispatcher &CurrentDispatcher();

        void Post(std::coroutine_handle<> h);

        void Push(std::coroutine_handle<> h);

        void Suspend(std::coroutine_handle<> h)
        {
        }

        void Post(Task<> &task)
        {
            Post(std::coroutine_handle<>(task));
        }
        template <typename T>
        void Post(Task<T> &task)
        {
            Post(std::coroutine_handle<>(task));
        }

        bool PumpMessages();

        using TimerListenerFn = void(void);
        using TimerHandle = uint64_t;
        TimerHandle AddTimer(std::chrono::milliseconds time, std::function<TimerListenerFn> callback);
        TimerHandle AddIntervalTimer(std::chrono::milliseconds interval, std::function<TimerListenerFn>);
        void CancelTimer(TimerHandle handle);

    private:
        TimerHandle nextTimerHandle = 0x10;

        struct DispatcherEntry
        {
            DispatcherEntry *pNext = nullptr;
            std::coroutine_handle<> handle;
            bool IsDone();
            void Execute();
        };
        DispatcherEntry *AllocEntry();
        void FreeEntry(DispatcherEntry *entry);

        DispatcherEntry *dispatcherQueue = nullptr;
        DispatcherEntry *lastEntry = nullptr;
        DispatcherEntry *freeEntries = nullptr;
    };

    template <typename TASK, typename T>
    inline bool TaskPromise<TASK, T>::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        //Dispatcher::CurrentDispatcher().Suspend(handle);
        return false;
    }
    template <typename TASK>
    inline bool TaskPromise<TASK>::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        //Dispatcher::CurrentDispatcher().Suspend(handle);
        return false;
    }

};