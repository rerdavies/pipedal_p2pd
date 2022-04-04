#pragma once
#include "CoTask.h"
#include <list>
#include <vector>
#include <functional>
#include <mutex>
#include "CoService.h"
#include "AsyncIo.h"

namespace cotask
{
#ifndef DOXYGEN
    namespace private_
    {
        class CoConditionaVariableImplementation;
    }
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
        [[nodiscard]] CoTask<bool> Wait(
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
        [[nodiscard]] CoTask<> Wait(std::function<bool(void)> conditionTest)
        {
            co_await Wait(NO_TIMEOUT, conditionTest);
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

        [[nodiscard]] CoTask<> Wait()
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
         * @brief Execute a function synchronously while the internal wait mutex is held, and return a value.
         * 
         * @tparam T 
         * @param testAction 
         * @return T The value returned by testAction.
         * 
         * testAction is called while protected by the internal mutex; but, unlike Notify(), no attempt is made to 
         * resume awaiting coroutines. Test() returns the value returned by executeAction.
         * 
         * e.g.:
         * 
         *      bool IsEmpty() {
         *             cv.Test([] { return queue.empty(); });
         *      }
         */
        template <class T>
        T Test(function<T(void)> testAction);

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
        [[nodiscard]] CoTask<> Lock();

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
        [[nodiscard]] CoTask<> Lock(CoMutex &mutex)
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

    /******* CoConditionVariable inlines ***************/

    template <class T>
    T CoConditionVariable::Test(function<T(void)> testAction)
    {
        std::lock_guard lock{mutex};
        return testAction();
    }

    inline void CoConditionVariable::Execute(std::function<void()> executeAction)
    {
        std::lock_guard lock { mutex};
        executeAction();
    }


} // namespace
