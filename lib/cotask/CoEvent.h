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
    namespace detail
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
     *           Task<> CoLock() 
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
     * *Lifetime Management*
     * 
     * Destroying a condition variable will cause all awaiters to throw a CoClosedException at their earliest possible convenience.
     * Handling of the CoClosedException can safely occur after the condition variable has been deleted; but after receiving a 
     * CoCloseException, the CoConditionVariable must not be accessed again as it will almost definitely have been deleted.
     * It is the caller's responsibility to manage the lifetime of associated data protected by the condition variable. Generally, the
     *  safest approach is to _not_ catch the CoClosedException, and treat it as a signal to terminate the entire coroutine thread as expediently and 
     * safely as possible.
     * 
     */
    class CoConditionVariable
    {
        friend class detail::CoConditionaVariableImplementation;

    public:
        ~CoConditionVariable();
        /**
         * @brief Suspend execution until conditionTest returns true.
         * 
         * @param conditionTest An optional condition check (see remarks).
         * @param timeout An optional timeout. 
         * @return Task<> 
         * @throws CoTimedOutException on timeout.
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
        [[nodiscard]] CoTask<> Wait(
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


        std::mutex&Mutex() { return mutex; }
    private:
        std::mutex mutex;
        bool ready = false;
        uint32_t deleted = 0;

        void CheckUseAfterFree();

        struct Awaiter
        {
            bool signaled = false;
            bool closed = false;
            CoConditionVariable *this_ = nullptr;
            std::exception_ptr exceptionPtr;
            std::function<bool(void)> conditionTest;
            std::chrono::milliseconds timeout;
            CoServiceCallback<void> *pCallback = nullptr;
            void UnhandledException()
            {
                exceptionPtr = std::current_exception();
            }
            void SetComplete()
            {
                auto t = pCallback;
                pCallback = nullptr;
                if (exceptionPtr)
                {
                    t->SetException(exceptionPtr);
                } else {
                    t->SetComplete();
                }
            }
        };
        std::vector<Awaiter *> awaiters;

        void AddAwaiter(Awaiter *awaiter);
        bool RemoveAwaiter(Awaiter *awaiter);
    };

    /**
     * @brief Prevents simultaneous execution of code or resources.
     * 
     * Only one coroutine can hold a lock on the CoMutex at any given time. CoMutexes are non-reentrant. Calling CoLock() on a CoMutex for which
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
         * Locks are non-reentrant. Calling CoLock() on a CoMutex that you already own will deadlock. Every call to CoLock() should be balanced
         * with a corresponding call to release.
         */
        [[nodiscard]] CoTask<> CoLock();

        /**
         * @brief Release the mutex.
         * 
         * Releasing the CoMutex allows other threads of execution to aquire the CoMutex. Any threads of execution that have been suspended 
         * in a call to CoLock() will be resumed.
         */
        void Unlock();

    private:
        CoConditionVariable cv;
        bool locked = false;
    };

    /**
     * @brief RAII container for CoMutex.
     * 
     * Coroutine equivalemt of std::lock_guard. Note that, unlike a std::lock_guard, a CoLock() call must be made (and co-awaited) after 
     * constructing the object.
     * 
     * Usage:
     *
     *     {
     *          CoLockGuard lock;
     *          co_await lock.CoLock(mutex);
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
                Terminate("You must call co_await CoLock() after construction.");
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
        [[nodiscard]] CoTask<> CoLock(CoMutex &mutex)
        {
            pMutex = &mutex;
            co_await mutex.CoLock();
            co_return;
        }

    private:
        CoMutex *pMutex = nullptr;
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
