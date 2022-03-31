#pragma once
#include "CoTask.h"
#include <list>
#include <vector>
#include <functional>
#include <mutex>
#include "CoService.h"
#include "AsyncIo.h"

namespace p2psession
{
#ifndef DOXYGEN
    namespace private_
    {
        class CoConditionaVariableImplementation;
    };
#endif

    /**
     * @brief Scheduling primitive based on Hoare condition variables.
     * 
     * CoConditionVariable provides a scheduling primitive used to build higher level scheduling operators (CoMutex, for example).
     * 
     * A CoConditionVariable has an internal mutex which is used to guarantee atomicity of scheduling operations when being used 
     * by concurrent threads of execution.
     * 
     * The Notify() and Wait() methods provide callbacks actions that are executed while the internal mutex is held. The Notify method will 
     * lock the internal mutex and call the notifyAction method. Doing so ensures that the notifyAction method completes synchronously with
     * respect to concurrent attempts to wait or executte timeouts. After calling the notifyAction method, Notify() will attempt to resume 
     * the coroutine of one suspended awaiter.
     * 
     * Wait() also provides a callback method as an argument, which is also executed while the internal mutex is held. If the callback 
     * returns true, the the current coroutine is allowed to procede. If it returns false, the current coroutine is
     * suspended and the the thread is added to the list of awaiters. During subsequent calls to Notify(), the callback method is called 
     * again. If it returns true, the coroutine of the awaiter is resumed.
     * 
     * CoConditionVariable allows surpisingly easy implementation of higher-level scheduling primitives. For example, a custom implementation 
     * of a coroutine mutex goes as follows.
     *  
     * 
     *      class MyMutex {
     *           CoConditionVariable cv;
     *           bool locked = false;
     *           Task<> Lock() 
     *           {
     *                cv.Wait([] () {
     *                     if (locked) return false; // suspend
     *                     locked = true;
     *                     return true;    // procede
     *                });
     *           }
     *           void Unlock() {
     *               cv.Notify([] () {
     *                     locked = false;
     *               });
     *           }
     *     };
     * 
     * 
     */
    class CoConditionVariable
    {
        friend class private_::CoConditionaVariableImplementation;

    public:
        /**
         * @brief Suspend execution until conditionTest returns true.
         * 
         * @param conditionTest An optional condition check (see remarks).
         * @param timeout An optional timeout. 
         * @return Task<bool> false if a timeout occurred (for invocationgs taking a timeout parameter) 
         * 
         * The conditionTest callback returns a boolean, indicating whether the 
         * current coroutine should be allowed to procede, or whether it should be suspended.
         * 
         * If the coroutine is suspended, subsequent calls to Notify() will call conditionTest again
         * and resume the coroutine if conditionTest returns true.
         * 
         * Each call to conditionTest is made while an internal wait mutex is held, thereby 
         * ensuring thread-safety against concurrent calls to Notify(), Wait(), or timeouts.
         * 
         * The default condition test is:
         * 
         *     [] () {
         *         if (this->ready){
         *               this->ready = false;
         *               return true; // procede.
         *         }
         *         return false; // suspend current coroutine.
         *     }

         */
        [[nodiscard]] Task<bool> Wait(
            std::chrono::milliseconds timeout,
            std::function<bool(void)> conditionTest = nullptr);

        /**
         * @brief Suspend execution until conditionTest returns true.
         * 
         * @param conditionTest An optional condition check (see remarks).
         * 
         * The conditionTest callback returns a boolean, indicating whether the 
         * current coroutine should be allowed to procede, or whether it should be suspended.
         * 
         * If the coroutine is suspended, subsequent calls to Notify() will call conditionTest again
         * and resume the coroutine if conditionTest returns true.
         * 
         * Each call to conditionTest is made while an internal wait mutex is held, thereby 
         * ensuring thread-safety against concurrent calls to Notify(), Wait(), or timeouts.
         *
         * The default implementation of the conditionTest is 
         * 
         *     [] () {
         *         if (this->ready){
         *               this->ready = false;
         *               return true; // procede.
         *         }
         *         return false; // suspend current coroutine.
         *     }
         * 
         * Used as the basis of other synchronization methods, such as CoMutex.
         */
        [[nodiscard]] Task<> Wait(std::function<bool(void)> conditionTest)
        {
            bool result = co_await Wait(NO_TIMEOUT, conditionTest);
            co_return;
        }
        /**
 * @brief Suspend execution of a courinte until a call to Notify() is made.
 *          * 
 * Waits using the default conditionTest, which is:
 * 
 *     [] () {
 *         if (this->ready){
 *               this->ready = false;
 *               return true; // procede.
 *         }
 *         return false; // suspend current coroutine.
 *     }
 */

        [[nodiscard]] Task<> Wait()
        {
            co_await Wait(std::chrono::milliseconds(NO_TIMEOUT), nullptr);
            co_return;
        }

        /**
         * @brief Wake up one waiters.
         * 
         * @param notifyAction An optional function to perform in protected context (see remarks)
         * 
         * notifyAction is called while the internal condition variable mutex is held. Operations performed in the 
         * notifyAction call are guaranteed to be thread-safe with respect to Wait() calls, other
         * Notify() calls, and timeouts.
         * 
         * The default notifyAction is 
         * 
         *     [] () { this->ready = true; }
         * 
         */
        void Notify(function<void(void)> notifyAction = nullptr);

        /**
         * @brief Wake up all watiers.
         * 
         * @param notifyAction An function to perform in protected context
         * 
         * Wake up current awaiters. notifyAction is called while the internal condition variable 
         * mutex is held. Operations performed in the  notifyAction call are guaranteed to be 
         * thread-safe with respect to Wait() calls, other Notify() calls, and timeouts.
         * 
         * Note that NotifyAll() only makes sense when used with non-default notify- and conditionTest 
         * parameters.
         */
        void NotifyAll(function<void(void)> notifyAction);
        /**
         * @brief Execute a function synchronously while the wait mutex is held.
         * 
         * @param executeAction the function to execute.
         * 
         * executeAction is called, but, unlike Notify(), no attempt is made to resume awaiting coroutines.
         * 
         * e.g.:
         *     
         *     void Clear() {
         *           cv.Execute([] () { queue.clear(); });
         *     }
         * 
         * 
         */
        void Execute(function<void(void)> executeAction);
        /**
         * @brief Execute a function synchronously while the internal wait mutex is held.
         * 
         * @tparam T 
         * @param executeAction 
         * @return T The value returned by executeAction.
         * 
         * executeAction is called, but, unlike Notify(), no attempt is made to resume awaiting coroutines.
         * 
         * e.g.:
         * 
         *      bool IsEmpty() {
         *             cv.Execute([] { return queue.empty(); });
         *      }
         */
        template <class T>
        T Execute(function<T(void)> executeAction);

    private:
        std::mutex mutex;
        bool ready = false;

        struct Awaiter
        {
            bool signaled = false;
            std::function<bool(void)> conditionTest;
            std::chrono::milliseconds timeout;
            CoServiceCallback<void> *pCallback = nullptr;
        };
        std::vector<Awaiter *> awaiters;

        void AddAwaiter(Awaiter *awaiter);
        bool RemoveAwaiter(Awaiter *awaiter);
    };

    /**
     * @brief Prevents simultaneous execution of code or resources.
     * 
     * Only one coroutine can hold a lock on the CoMutex at any given time. CoMutexes are non-reentrant. Calling Lock() on a CoMutex for which
     * you already hold a lock will deadlock.
     * 
     */
    class CoMutex
    {
    public:
        /**
         * @brief Aquire the mutex.
         * 
         * @return Task<> co_await the return value.
         * 
         * If another coroutine has aquired the CoMutex, the current coroutine will be suspended until 
         * the other thread calls UnLock().
         * 
         * Locks are non-reentrant. Calling Lock() on a CoMutex that you already own will deadlock. Every call to Lock() should be balanced
         * with a corresponding call to release.
         */
        [[nodiscard]] Task<> Lock();

        /**
         * @brief Release the mutex.
         * 
         * Releasing the CoMutex allows other threads of execution to aquire the CoMutex. Any threads of execution that have been suspended 
         * in a call to Lock() will be resumed.
         */
        void Unlock();

    private:
        CoConditionVariable cv;
        bool locked = false;
    };

    /**
     * @brief RAII container for CoMutex.
     * 
     * Coroutine equivalemt of std::lock_guard. Note that, unlike a std::lock_guard, a Lock() call must be made (and co-awaited) after 
     * constructing the object.
     * 
     * Usage:
     *
     *     {
     *          CoLockGuard lock;
     *          co_await lock.Lock(mutex);
     * 
     *          ....
     *     }  // Unlock call made by the destructor.
     * 
     */
    class CoLockGuard
    {
    public:
        ~CoLockGuard()
        {
            if (!pMutex)
            {
                //throw std::exception("You must call Lock() after constructing.")
                cerr << "You must call co_await Lock() after construction." << endl;
                std::terminate();
            }
            pMutex->Unlock();
        }
        /**
         * @brief Task a lock on the supplied CoMutex.
         * 
         * @param mutex The CoMutex on which a Lock() will be taken.
         * @return Task<> 
         * 
         * An Unlock() call will be made on the CoMutext when CoLockGuard is destructed.
         */
        [[nodiscard]] Task<> Lock(CoMutex &mutex)
        {
            pMutex = &mutex;
            co_await mutex.Lock();
            co_return;
        }

    private:
        CoMutex *pMutex = nullptr;
    };

    class CoQueueClosedException : public CoException
    {
    public:
        using base = CoException;

        CoQueueClosedException()
        {
        }
        const char *what() const noexcept
        {
            return "Queue closed.";
        }
    };
    /**
     * @brief An thread-safe coroutine queue containing elements of type T.
     * 
     * @tparam T Type of objects contained in the quueue.
     * 
     * The maximum length of a CoBlockingQueue is unbounded. For a queue that has a maximum number 
     * of entries, see CoBlockingQueue.
     */
    template <class T>
    class CoBlockingQueue
    {
    public:
        using item_type = T;

        CoBlockingQueue(size_t maxLength);

        /**
         * @brief Copy a value into the queue.
         * 
         * @param value 
         * @param timeout (optional) Time to wait for space in the queue.
         * @return Task<> 
         * @throws CoTimeoutException on timeout.
         * 
         * The current coroutine is suspended if there is no space in the queue. 
         */
        Task<> Push(const T &value, std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Move a value into the queue.
         * 
         * @param value 
         * @param timeout (optional) How long to wait for space to become available inthe queue.
         * @return Task<> 
         * @throws CoTimeoutException on timeout.
         * 
         * The current coroutine is suspended if there is no space in the queue. 
         */
        Task<> Push(T &&value, std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Take a value from the queue.
         * 
         * @param timeout How long to wait for a value to become available.
         * @return Task<T> 
         * @throws CoTimeoutException on timeout
         * @throws CoQueueClosedException if Close() has been called.
         * 
         * A CoTimeoutException is thrown if the timeout expires.
         */
        Task<T> Take(std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Close the queue.
         * 
         * Subsequent attempts to Take() from the queue will throw a CoQueueClosedException.
         * 
         */
        void Close();
        /**
         * @brief Is the queue empty?
         * 
         * @return true if the queue is empty.
         * @return false if the queue is not empty.
         */
        bool IsEmpty();

    private:
        size_t maxLength;
        bool queueEmpty = true;
        bool queueFull = false;
        CoConditionVariable pushCv;
        CoConditionVariable takeCv;
        using queue_type = std::queue<T, std::list<T>>;
        queue_type queue;
        bool closed = false;
    };

    /******* CoConditionVariable inlines ***************/

    template <class T>
    T CoConditionVariable::Execute(function<T(void)> executeAction)
    {
        std::lock_guard lock{mutex};
        return executeAction();
    }
    /*****  CoBlockingQueue inlines ****************************/
    template <typename T>
    Task<> CoBlockingQueue<T>::Push(const T &value, std::chrono::milliseconds timeout)
    {
        co_await pushCv.Wait(
            timeout,
            [this, &value]() {
                if (queue.size() == this->maxLength)
                {
                    queueFull = true;
                    return false;
                }
                queue.push(value);
                return true;
            });
        takeCv.Notify([this]() {
            queueEmpty = false;
        });
        co_return;
    }
    template <typename T>
    Task<> CoBlockingQueue<T>::Push(T &&value, std::chrono::milliseconds timeout)
    {
        co_await pushCv.Wait(
            timeout,
            [this, &value]() {
                if (queue.size() == this->maxQueueSize)
                {
                    queueFull = true;
                    return false;
                }
                queue.push_back(std::move(value));
                return true;
            });
        takeCv.Notify([this]() {
            queueEmpty = false;
        });
        co_return;
    }
    template <typename T>
    Task<T> CoBlockingQueue<T>::Take(std::chrono::milliseconds timeout)
    {
        T value;
        co_await takeCv.Wait(
            timeout,
            [this, &value]() {
                if (queue.empty())
                {
                    queueEmpty = true;
                    if (closed)
                    {
                        throw CoQueueClosedException();
                    }
                    return false;
                }
                value = std::move(queue.front());
                queue.pop();
                return true;
            });

        pushCv.Notify(
            [this]() {
                queueFull = false;
            });

        co_return value;
    }

    template <typename T>
    bool CoBlockingQueue<T>::IsEmpty()
    {
        return takeCv.Execute([this] {
            return queueEmpty;
        });
    }

    template <typename T>
    void CoBlockingQueue<T>::Close()
    {
        takeCv.NotifyAll(
            [this] {
                this->closed = true;
            }
        );
    }
    template <typename T>
    CoBlockingQueue<T>::CoBlockingQueue(size_t maxLength)
    :maxLength(maxLength)
    {

    }

} // namespace
