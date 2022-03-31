#pragma once

#include <memory>
#include <coroutine>
#include <functional>
#include <exception>
#include "CoTask.h"

namespace p2psession
{
    using namespace std;  // for std::chrono operator "" extensions.

/**************************************************
 
CoServices provide a realtively painless way to implement coroutine methods
in native code. The CoService wrapper provides the not-insubstantial glue and
code to wrap native code. 

Authors must provide code to copy method parametes for use off the thread;  
and two calls to marshal the thread of execution off the coroutine thread, 
and back on to the couroutine thread when done. For method that have a co_return
 alue  authors must provide a class of the following form:

    template <typename T>
    class MyServiceImplementation {
        using return_type=T;
        // Some method parameters stored here.
        ....
        // ****
        void Execute(CoServiceCallback<RETURN_TYPE> *pCallback);
        bool CancelExecute(CoServiceCallback<RETURN_TYPE> *pCallback);
    };

For methods returning void, the form is:

    class MyVoidServiceImplementation {
        // Some method parameters stored here.
        ....
        // ****

        void Execute(CoServiceCallback<void> *pCallback);
        bool CancelExecute(CoServiceCallback<void> *pCallback);
    };

The implentation is hooked up using code that follows this form:


    auto CoReadSocket(char *buffer, size_t count) {

        // My implementation of the CoService.
        class MySocketImplementation {
        public: 
            using return_type = size_t; // the type returned by co_await.

            char *buffer;   // stores copies of the original method parameters.
            int count;      // stores copies of the original method parameters.

            handle_t ioHandle = 0;
            CoServiceCallback<> *pCallback = nullptr;

            void Execute(CoServiceCallback<> *pCallback)
            {
                this->pCallback = pCallback; // save a copy
                // delegate to another scheduler (for example). Your code here.
                ioHandle = gAsyncIoDispatcher->read()
                        this->buffer,this->count, [&this] (int nRead) -> { this->OnRequestComplete(nRead); });
            }

            bool CancelExecute(CoServiceCallback<> *pCallback)
            {
                if (ioHandle)
                {
                    // return false only if the call is in flight and can't be cancelled.
                    bool cancelled = gAsyncIoDispatcher->cancel(ioHandle);
                    ioHandle = 0;
                    if (cancelled)
                    {
                        this->pCallback = nullptr; // no longer valid.
                    }
                    return cancelled;
                } else {
                    this->pCallback = nullptr; // no longer valid.
                    return true;
                {}
            }

            // what to do when your scheduler returns a value (by whatever means
            void OnrequestComplete(int nRead)
            {
                if (nRead < 0) {
                    // manucture  an std::exception_ptr, which can be (miraculously) marshalled across thread boundaries!
                    std::exception_ptr exceptionPointer;
                    try {
                        throw MyException("I/O failed.");
                    } catch (const std::exception &e) {
                        exceptionPtr = std::current_exception(); 
                    }
                    // Causes the original exception to be thrown on the coroutine thread while 
                    // leaving co_await.

                    pCallback->SetException(std::invalid_argument("I/O operation failed."));

                } else {
                    // resumes execution of the original coroutine with a co_await return value of nRead.
                    // if your return type is void, call SetResult() instead.
                    pCallback->SetResult(nRead); 
                    // we will get deleted by wrapper after we return.
                }
            }

        };
        // Create the wrapper class.
        using Awaitable = CoService<MySocketImplementation>;

        // The awaitable wrapper that can be co_await-ed.
        Awaitable awaitable {};
        // copy in method parameters. Our implementaion is a public base class of 
        // Awaitable so we can copy in whatever state we need. 
        awaitable.buffer = buffer;
        awaitable.coount = count;

        // set it free to run!
        return awaitable;
    };

It looks a bit daunting, but over half of the code is plausibly real implementation of ansyncrhonous i/o. 
Essentially, two methods to implement:  MySocketImplementation::Execute(), and MySocketImplementation::CancelExecute(),
and some glue to hook things up.

Wrapper code will call `Execute` once the coroutine thread of execution has been 
saved. The pCallback parameter provides callback methods that can resume the coroutine
thread, returning a value, or throwing an exception on the resumed coroutine thread,
depending on which method is used to resume execution.

In response to the Execute call, the author of the CoService implementation is 
expected to marshal the data (the pCallback and required parameters) onto another thread or scheduler
process in order to perform work. 

Examples of scheduler process yu might want to marshal to: an asynchronous IO chain 
of execution; a non-coroutine worker thread; or a chain of notifications or promises
executed on the main thread or CoDispatcher foreground thread. 

Wrapper code may call CancelExecute at any time up until a callback to a CoServiceCallback 
method is made. The pCallback method will have the same value as the pCallback value
provided to Execute. Any work initiated by Execute should be stopped as 
expediently as possble, and one CancelExecute returns, NO further use of the original
pCallback should be made, as underlying memory will have been deleted. 

Note that CancelExecute gets called extremely rarely. Calls occur either because a 
shutdown is in progress; or becase the original coroutine (or one if its 
parents) has been abnormally and forcibly terminated by a mechanism provide
in some future release of CoDispatcher. (It's not called as part of 
exception handling, since the coroutine thread has been suspended 
until the service returns control).)

should be cancelled, and any instances of the pCallback should be discarded. The 
pCallback data structures will be deleted by wrapper code; so any remaining copies 


Once done, pCallback->ReturnValue() will resume the 
original suspended coroutine either on the foreground thread (if that's where it started)
or on an undetermined thread in the scheduler pool (not neccessarily the original thread).

pCallback methods can be called on any thread of execution. Wrapper code takes care of 
the details of getting the data back to the original coroutine on an appropriate thread
-- either on the foreground thread (if that's where it started)
or on an undetermined thread in the scheduler pool (not neccessarily the original thread).

THREAD SAFETY

Execute, and CancelExecute run mutually exclusively (they're mutex protected). CancelExecute 
is guaranteed to run after Execute, even when running in the background thread pool.

There is, however, a race condition between the results callback and the CancelExecute 
call. It is possible (and sufficient) to use a mutex in the implementation class to
enure that CancelExecute, and your value callback run mutually exclusively. However,
it's quite possible that the  scheduler you're connecting to  may provide mutual
exclusion. Check carefully to see whether that's the case. (The cancel call must 
block if there's a notification of a result in progress). The actual posting 
of results by the wrapper code is quite lightweight. It wouldn't be innapropriate
to hold off the cancel call with a spinlock (especially given how rarely cancel 
events occur). And if your host os's mutexes are truly very lightweight, then
go ahead and use mutexes. Windows mutexes, for example don't allocate wait 
handles until there's an actual contention. Which is going to be incredibly
rare.

Even with appropriate mutex protecteion, you should still be prepared to deal
with the case in which a cancel call occurs even after completion notification 
has been given.


***********************************************/

    enum ServiceState
    {
        Idle,
        Executing,
        ExecutingTimedOut,
        ExecutingResumed,
        ExecutingTimedOutAndResumed,
        Executed,
        ResumedCancellingTimeout, // call made to cancel the timeout
        ResumedTimeoutInFlight,   // cancelling the timeout failed, so Cancel must be in flight.
        TimedOutResumeInFlight,   // cancelling the resume failed, so the resume must be in flight/

        Resuming,
        Resumed,
    };

    template <typename T>
    class CoServiceCallback;

    template <typename T>
    class CoServiceCallback
    {
    public:
        virtual void SetResult(T &&value) = 0;
        virtual void SetException(std::exception_ptr exceptionPtr) = 0;
        virtual void RequestTimeout(std::chrono::milliseconds timeout) = 0;
    };

    template <>
    class CoServiceCallback<void>
    {
    public:
        virtual void SetComplete() = 0;
        virtual void SetException(std::exception_ptr exceptionPtr) = 0;
        virtual void RequestTimeout(std::chrono::milliseconds timeout) = 0;
    };

    // template <typename T>
    // concept IsTypedCoServiceImplementation = requires(
    //     T t,
    //     typename T::return_type x,
    //     CoServiceCallback<typename T::return_type> *callback)
    // {
    //     {t.Execute(callback)} -> std::same_as<void>;
    //     {t.CancelExecute(callback)} -> std::same_as<bool>;
    // };

    template <typename T>
    concept IsTypedCoServiceImplementation = 

    requires { typename T::return_type; } 
    && !std::same_as<typename T::return_type,void>
    && (!std::same_as<typename T::return_type,void>)
    && requires (T t,  CoServiceCallback<typename T::return_type> *callback)
    { {  t.Execute(callback) } -> std::same_as<void>;} 
    
    && requires (T *this_,  CoServiceCallback<typename T::return_type> *callback)
    { { this_->CancelExecute(callback)} -> std::same_as<bool>; }
    ;

    template <typename T>
    concept IsVoidCoServiceImplementation = requires(
        T t,
        CoServiceCallback<void> *callback)
    {
        {t.Execute(callback)};
        {t.CancelExecute(callback)} -> std::same_as<bool>;
    };

    // The void-returning case.
    template <typename SERVICE_IMPLEMENTATION>
    class CoServiceBase : public SERVICE_IMPLEMENTATION
    {
    protected:
        using service_implementation = SERVICE_IMPLEMENTATION;

        static std::mutex serviceStateMutex;
        ServiceState serviceState = ServiceState::Idle;
        void SetServiceState(ServiceState serviceState)
        {
            std::unique_lock lock{serviceStateMutex};
            this->serviceState = serviceState;
        }
        ServiceState GetServiceState()
        {
            std::unique_lock lock{serviceStateMutex};
            return serviceState;
        }

        void throwInvalidState(const char *action)
        {
            std::cout << "ERROR: CoService Not in a valid state. (" << action << "," << ((int)serviceState) << ")" << std::endl;
            cout.flush();
            std::terminate();
        }

        void OnRequestTimeout(std::chrono::milliseconds timeout)
        {
            if (serviceState != ServiceState::Executing && serviceState != ServiceState::ExecutingResumed)
            {
                throwInvalidState("OnRequestTimeout must be called in Execute()");
            }
            if (timeout == 0ms)
            {
                return;
            }
            if (timeout < 0ms)
            {
                // required by test cases. Throw the timeout in the execute call.
                // Required to conver the complete state machine. 
                // (specifically the Executing->ExecutingTimedOut state transition which is insanely rare,
                // but nonetheless possible if executing in the thread pool and Bad Stuff Happened. )
                OnTimedOut();
                return;
            }
            this->timerHandle = CoDispatcher::CurrentDispatcher().PostDelayedFunction(timeout,
                                                                  [this]() {
                                                                      OnTimedOut();
                                                                  });
            timeoutRequested = true;
        }
        bool CancelTimeout()
        {
            if (!timeoutRequested)
                return true;
            bool cancelled = CoDispatcher::CurrentDispatcher().CancelDelayedFunction(timerHandle);
            timeoutRequested = false;
            timerHandle = 0;
            return cancelled;
        }
        virtual bool CancelResume() = 0;

    private:
        void Resume()
        {
            std::unique_lock lock{serviceStateMutex};
            if (serviceState != ServiceState::Resuming)
            {
                throwInvalidState("Resume");
            }
            serviceState = ServiceState::Resumed;

            // prepare to lose *this...
            auto handle = this->suspendedHandle;
            auto dispatcher = this->foregroundDispatcher;
            bool isForeground = this->isForeground;

            this->suspendedHandle = 0;
            this->foregroundDispatcher = nullptr;

            // From this point on, *this is no longer valid.
            if (isForeground)
            {
                dispatcher->Post(handle);
            }
            else
            {
                dispatcher->PostBackground(handle);
            }
        }

    protected:
        void OnResume()
        {
            std::unique_lock lock{serviceStateMutex};

            switch (serviceState)
            {
            case ServiceState::Executing:
                serviceState = ServiceState::ExecutingResumed;
                break;
            case ServiceState::ExecutingTimedOut:
                serviceState = ServiceState::ExecutingTimedOutAndResumed;
                break;
            case ServiceState::Executed:
            {
                bool cancelledTimeout = CancelTimeout();
                if (cancelledTimeout)
                {
                    serviceState = ServiceState::Resuming;
                    lock.unlock();
                    Resume();
                    return;
                }
                else
                {
                    serviceState = ServiceState::ResumedTimeoutInFlight;
                }
            }
            break;
            case ServiceState::TimedOutResumeInFlight:
                this->hasTimeout = false; // let the resume win!
                this->serviceState = ServiceState::Resuming;
                lock.unlock();
                Resume();
                return;
            default:
                throwInvalidState("OnResume");
            }
        }

        void OnExecuted()
        {
            std::unique_lock lock{serviceStateMutex};
            switch (serviceState)
            {
            case ServiceState::Executing:
                serviceState = Executed;
                break;
            case ServiceState::ExecutingTimedOut:
            {
                bool canceledResume = CancelResume();
                if (canceledResume)
                {
                    serviceState = ServiceState::Resuming;
                    lock.unlock();
                    Resume();
                    return;
                }
                else
                {
                    serviceState = ServiceState::TimedOutResumeInFlight;
                }
            }
            break;
            case ServiceState::ExecutingResumed:
            {
                bool cancelledTimeout = CancelTimeout();
                if (cancelledTimeout)
                {
                    serviceState = ServiceState::Resuming;
                    lock.unlock();
                    Resume();
                    return;
                }
                else
                {
                    serviceState == ServiceState::ResumedTimeoutInFlight;
                }
            }
            break;
            case ServiceState::ExecutingTimedOutAndResumed:
                serviceState = ServiceState::Resuming;
                lock.unlock();
                Resume();
                return;
            default:
                throwInvalidState("OnExecuted");
            }
        }
        void OnTimedOut()
        {
            std::unique_lock lock{serviceStateMutex};
            switch (serviceState)
            {
            case ServiceState::Executing:
                this->hasError = true;
                this->hasTimeout = true;
                this->serviceState = ServiceState::ExecutingTimedOut;
                break;
            case ServiceState::ExecutingResumed:
                this->serviceState = ServiceState::ExecutingTimedOutAndResumed;
                break;
            case ServiceState::Executed:
            {
                this->hasError = true;
                this->hasTimeout = true;
                bool resumeCancelled = CancelResume();
                if (resumeCancelled)
                {
                    this->serviceState = ServiceState::Resuming;
                    lock.unlock();
                    Resume();
                    return;
                }
                else
                {
                    this->serviceState = ServiceState::TimedOutResumeInFlight;
                }
            }
            break;
            case ServiceState::ResumedTimeoutInFlight:
            {
                this->serviceState = ServiceState::Resuming;
                lock.unlock();
                Resume();
                return;
            }
            default:
                throwInvalidState("onTimedout");
            }
        }

    public:
        CoServiceBase();
        virtual ~CoServiceBase();

        // coroutine contract implementation

        bool await_ready() const noexcept { return false; }

    protected:
        std::coroutine_handle<> suspendedHandle;
        bool suspended = false;
        bool resumed = false;
        bool timeoutRequested = false;
        uint64_t timerHandle = 0;

        bool hasError = false;
        bool hasTimeout = false;
        bool cancelled = false;
        std::exception_ptr exceptionPtr;

        bool isForeground = true;
        CoDispatcher *foregroundDispatcher = nullptr;
    };

    template <typename SERVICE_IMPLEMENTATION>
    class VoidCoServiceBase : public CoServiceBase<SERVICE_IMPLEMENTATION>, private CoServiceCallback<void>
    {

    private:
        virtual void SetComplete()
        {
            CoServiceBase<SERVICE_IMPLEMENTATION>::OnResume();
        }

    public:
        using return_type = void;
        using service_implementation = SERVICE_IMPLEMENTATION;
        using service_base = CoServiceBase<SERVICE_IMPLEMENTATION>;

        void await_resume() const;

        virtual bool CancelResume()
        {
            return SERVICE_IMPLEMENTATION::CancelExecute((CoServiceCallback<void> *)this);
        }
        void await_suspend(std::coroutine_handle<> coroutine) noexcept
        {
            service_base::suspended = true;
            service_base::suspendedHandle = coroutine;
            service_base::SetServiceState(ServiceState::Executing);
            service_implementation::Execute((CoServiceCallback<void> *)this);
            service_base::OnExecuted();
        }
    };

    template <typename SERVICE_IMPLEMENTATION, typename RETURN_TYPE>
    class TypedCoServiceBase : public CoServiceBase<SERVICE_IMPLEMENTATION>, private CoServiceCallback<RETURN_TYPE>
    {
    public:
        using return_type = RETURN_TYPE;
        using service_implementation = SERVICE_IMPLEMENTATION;
        using service_base = CoServiceBase<SERVICE_IMPLEMENTATION>;
        // coroutine implementation

        virtual void SetResult(return_type &&value);
        return_type await_resume() const;

        virtual bool CancelResume()
        {
            return SERVICE_IMPLEMENTATION::CancelExecute(static_cast<CoServiceCallback<RETURN_TYPE> *>(this));
        }

        void await_suspend(std::coroutine_handle<> coroutine) noexcept
        {
            service_base::suspended = true;
            service_base::suspendedHandle = coroutine;
            service_base::SetServiceState(ServiceState::Executing);

            service_implementation::Execute(static_cast<CoServiceCallback<RETURN_TYPE> *>(this));

            service_base::OnExecuted();
        }

    private:
        RETURN_TYPE value;
    };

    template <typename SERVICE_IMPLEMENTATION>
    requires(IsVoidCoServiceImplementation<SERVICE_IMPLEMENTATION> || IsTypedCoServiceImplementation<SERVICE_IMPLEMENTATION>) class CoService;

    template <IsTypedCoServiceImplementation SERVICE_IMPLEMENTATION>
    class CoService<SERVICE_IMPLEMENTATION> : public TypedCoServiceBase<SERVICE_IMPLEMENTATION, typename SERVICE_IMPLEMENTATION::return_type>
    {
    public:
        using return_type = typename SERVICE_IMPLEMENTATION::return_type;
        using service_implementation = SERVICE_IMPLEMENTATION;
        using service_base = TypedCoServiceBase<SERVICE_IMPLEMENTATION, typename SERVICE_IMPLEMENTATION::return_type>;

    public:
        CoService()
        {
        }
        virtual ~CoService();

        void RequestTimeout(std::chrono::milliseconds timeout)
        {
            CoServiceBase<SERVICE_IMPLEMENTATION>::OnRequestTimeout(timeout);
        }
        void SetException(std::exception_ptr exceptionPtr);

        bool await_ready() const noexcept { return false; }

    protected:
        // Methods provided by SERVICE_IMPLEMENTATION base calls.
        // virtual void Execute(CoServiceCallback<RETURN_TYPE> *callback) = 0;
        // virtual void CancelExecuteCallback(CoServiceCallback<RETURN_TYPE> *callback) = 0;
    };
    template <IsVoidCoServiceImplementation SERVICE_IMPLEMENTATION>
    class CoService<SERVICE_IMPLEMENTATION> : public VoidCoServiceBase<SERVICE_IMPLEMENTATION>
    {

    public:
        using return_type = void;
        using service_implementation = SERVICE_IMPLEMENTATION;
        using service_base = VoidCoServiceBase<SERVICE_IMPLEMENTATION>;
        CoService()
        {
        }
        virtual ~CoService();

        void RequestTimeout(std::chrono::milliseconds timeout)
        {
            CoServiceBase<SERVICE_IMPLEMENTATION>::OnRequestTimeout(timeout);
        }
        void SetException(std::exception_ptr exceptionPtr);

        bool await_ready() const noexcept { return false; }

    protected:
        // Methods provided by SERVICE_IMPLEMENTATION base calls.
        // virtual void OnSuspend(CoServiceCallback<RETURN_TYPE> *callback) = 0;
        // virtual void CancelExecuteCallback(CoServiceCallback<RETURN_TYPE> *callback) = 0;
    };

    //******************** implementation ***************

    template <typename SERVICE_IMPLEMENTATION>
    CoServiceBase<SERVICE_IMPLEMENTATION>::CoServiceBase()
    {
        CoDispatcher &dispatcher = CoDispatcher::CurrentDispatcher();

        this->foregroundDispatcher = dispatcher.GetForegroundDispatcher();
        this->isForeground = dispatcher.IsForeground();
    }

    template <typename SERVICE_IMPLEMENTATION, typename RETURN_TYPE>
    void TypedCoServiceBase<SERVICE_IMPLEMENTATION, RETURN_TYPE>::SetResult(RETURN_TYPE &&value)
    {
        value = std::move(value);
        CoServiceBase<SERVICE_IMPLEMENTATION>::OnResume();
    }

    template <IsTypedCoServiceImplementation SERVICE_IMPLEMENTATION>
    void CoService<SERVICE_IMPLEMENTATION>::SetException(std::exception_ptr exceptionPtr)
    {
        this->CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr = exceptionPtr;
        this->CoServiceBase<SERVICE_IMPLEMENTATION>::hasError = true;
        CoServiceBase<SERVICE_IMPLEMENTATION>::OnResume();
    }
    template <IsVoidCoServiceImplementation SERVICE_IMPLEMENTATION>
    void CoService<SERVICE_IMPLEMENTATION>::SetException(std::exception_ptr exceptionPtr)
    {
        this->CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr = exceptionPtr;
        this->CoServiceBase<SERVICE_IMPLEMENTATION>::hasError = true;
        CoServiceBase<SERVICE_IMPLEMENTATION>::OnResume();
    }

    template <typename SERVICE_IMPLEMENTATION>
    void VoidCoServiceBase<SERVICE_IMPLEMENTATION>::await_resume() const
    {
        if (CoServiceBase<SERVICE_IMPLEMENTATION>::hasError)
        {
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr)
            {
                std::rethrow_exception(CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr);
            }
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::hasTimeout)
            {
                throw CoTimedOutException();
            }
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::cancelled)
            {
                throw CoCancelledException();
            }
        }
        return;
    }

    template <typename SERVICE_IMPLEMENTATION, typename RETURN_TYPE>
    RETURN_TYPE TypedCoServiceBase<SERVICE_IMPLEMENTATION, RETURN_TYPE>::await_resume() const
    {
        if (CoServiceBase<SERVICE_IMPLEMENTATION>::hasError)
        {
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr)
            {
                std::rethrow_exception(CoServiceBase<SERVICE_IMPLEMENTATION>::exceptionPtr);
            }
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::hasTimeout)
            {
                throw CoTimedOutException();
            }
            if (CoServiceBase<SERVICE_IMPLEMENTATION>::cancelled)
            {
                throw CoCancelledException();
            }
        }
        return value;
    }

    template <typename SERVICE_IMPLEMENTATION>
    CoServiceBase<SERVICE_IMPLEMENTATION>::~CoServiceBase()
    {
    }

    template <IsVoidCoServiceImplementation SERVICE_IMPLEMENTATION>
    CoService<SERVICE_IMPLEMENTATION>::~CoService()
    {
        if (CoServiceBase<SERVICE_IMPLEMENTATION>::serviceState != ServiceState::Resumed 
        && CoServiceBase<SERVICE_IMPLEMENTATION>::serviceState != ServiceState::Idle)
        {
            CoServiceBase<SERVICE_IMPLEMENTATION>::throwInvalidState("Destroyed in invalid state.");
            std::terminate();
        }
    }
    template <IsTypedCoServiceImplementation SERVICE_IMPLEMENTATION>
    CoService<SERVICE_IMPLEMENTATION>::~CoService()
    {
        if (CoServiceBase<SERVICE_IMPLEMENTATION>::serviceState != ServiceState::Resumed 
        && CoServiceBase<SERVICE_IMPLEMENTATION>::serviceState != ServiceState::Idle)
        {
            CoServiceBase<SERVICE_IMPLEMENTATION>::throwInvalidState("Destroyed in invalid state. ");
            std::terminate();
        }
    }

    template <typename SERVICE_IMPLEMENTATION>
    std::mutex CoServiceBase<SERVICE_IMPLEMENTATION>::serviceStateMutex;

} // namespace