#include "cotask/CoEvent.h"

using namespace cotask;

namespace cotask
{
    #ifndef DOXYGEN
    namespace private_
    {
        // friend of CoConditionaVariable
        class CoConditionaVariableImplementation
        {
        public:
            static auto Wait_(
                CoConditionVariable *this_,
                std::function<bool(void)> condition,
                std::chrono::milliseconds timeout = std::chrono::milliseconds(-1))
            {
                struct Implementation : CoConditionVariable::Awaiter
                {
                    using return_type = void;

                    CoConditionVariable *this_;
                    void Execute(CoServiceCallback<void> *pCallback)
                    {
                        bool signaled = this->signaled;
                        {
                            std::lock_guard lock{this_->mutex};
                            if (!signaled)
                            {
                                if (this_->ready)
                                {
                                    signaled = true;
                                    this_->ready = false;
                                }
                            }
                            if (this->conditionTest != nullptr)
                            {
                                try {
                                    signaled = this->conditionTest();
                                } catch (const std::exception &e)
                                {
                                    pCallback->SetException(std::current_exception());
                                    return;
                                }
                            }
                        }
                        if (signaled)
                        {
                            pCallback->SetComplete();
                            return;
                        }
                        if (this->timeout != NO_TIMEOUT)
                        {
                            pCallback->RequestTimeout(this->timeout);
                        }
                        this->pCallback = pCallback;
                        this_->AddAwaiter(this);
                    }

                    bool CancelExecute(CoServiceCallback<void> *pCallback)
                    {
                        return this_->RemoveAwaiter(this);
                    }
                };
                CoService<Implementation> awaiter;
                awaiter.this_ = this_;
                awaiter.conditionTest = condition;
                awaiter.timeout = timeout;
                return awaiter;
            }
        };
        #endif
    }
}

void CoConditionVariable::AddAwaiter(Awaiter *awaiter)
{
    std::lock_guard lock{mutex};
    awaiters.push_back(awaiter);
}
bool CoConditionVariable::RemoveAwaiter(Awaiter *awaiter)
{
    std::lock_guard lock{mutex};
    for (auto i = awaiters.begin(); i != awaiters.end(); ++i)
    {
        if ((*i) == awaiter)
        {
            awaiters.erase(i);
            return true;
        }
    }
    return false;
}

CoTask<bool> CoConditionVariable::Wait(
    std::chrono::milliseconds timeout,
    std::function<bool(void)> condition
    )
{
    try
    {
        co_await private_::CoConditionaVariableImplementation::Wait_(this, condition, timeout);
        co_return true;
    }
    catch (const CoTimedOutException &e)
    {
        co_return false;
    }
}

void CoConditionVariable::NotifyAll(function<void(void)> notifyAction)
{
    // manage the queue under the mutex.
    std::vector<Awaiter*> readyAwaiters;
    {
        std::unique_lock lock{mutex};
        if (notifyAction)
        {
            notifyAction();
        }

    
        for (auto i = awaiters.begin(); i != awaiters.end(); /**/)
        {   
            bool ready = true;
            auto awaiter = *i;
            if (awaiter->conditionTest)
            {
                try {
                    ready = awaiter->conditionTest();
                } catch (const std::exception &e)
                {
                    awaiter->UnhandledException();
                    ready = true;
                }
            }
            if (ready)
            {
                readyAwaiters.push_back(awaiter);
                i = awaiters.erase(i);
            } else {
                ++i;
            }
        }
    }
    // do the callbacks without the mutx.
    for (auto i = readyAwaiters.begin(); i != readyAwaiters.end(); ++i)
    {
        Awaiter *pAwaiter = (*i);
        pAwaiter->SetComplete();
    }
}

void CoConditionVariable::Notify(std::function<void(void)> action)
{
    std::unique_lock lock{mutex};
    if (action)
    {
        action();
    }
    if (awaiters.size() != 0)
    {
        Awaiter *awaiter = *(awaiters.begin());
        bool ready = true;
        if (awaiter->conditionTest)
        {
            try {
                ready = awaiter->conditionTest();
            } catch (const std::exception &e)
            {
                awaiters.erase(awaiters.begin());
                if (awaiter->pCallback != nullptr)
                {
                    auto t = awaiter->pCallback;
                    awaiter->pCallback = nullptr;
                    lock.unlock();
                    t->SetException(std::current_exception());
                    return;
                }
            }
        }
        if (ready)
        {
            awaiters.erase(awaiters.begin());
            if (awaiter->pCallback != nullptr)
            {
                auto t = awaiter->pCallback;
                awaiter->pCallback = nullptr;
                lock.unlock();
                t->SetComplete();
                return;
            }
            else
            {
                awaiter->signaled = true;
                return;
            }
        }
    }
    else
    {
        this->ready = true;
    }
}

CoTask<> CoMutex::CoLock()
{
    co_await cv.Wait([this] {
        if (locked)
        {
            return false; // suspend.
        }
        locked = true;
        return true; // resume with the lock
    });
    co_return;
}
void CoMutex::Unlock()
{
    cv.Notify([this] {
        locked = false;
    });
}