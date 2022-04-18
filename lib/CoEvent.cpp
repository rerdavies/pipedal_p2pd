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

#include "cotask/CoEvent.h"
#include <stdexcept>

using namespace cotask;

namespace cotask
{
#ifndef DOXYGEN
    namespace detail
    {
        // friend of CoConditionaVariable
        class CoConditionaVariableImplementation
        {
        public:
            static auto Wait_(
                CoConditionVariable *this_,
                std::function<bool(void)> condition,
                std::chrono::milliseconds timeout = NO_TIMEOUT)
            {
                struct Implementation : CoConditionVariable::Awaiter
                {
                    ~Implementation()
                    { // debug hook to verify that we get deleted on close.
                        if (!closed && this_ != nullptr)
                        {
                            {
                                std::lock_guard lock {this_->mutex};
                                bool result = this_->RemoveAwaiter(this);
                                if (result)
                                {
                                    Dispatcher().Log().Warning("Orphaned awaiter.");
                                }
                            }
                        }
                        this_ = nullptr;
                    }

                    using return_type = void;

                    void Execute(CoServiceCallback<void> *pCallback)
                    {

                        {
                            if (!this_)
                                return;
                            if (this_->deleted)
                            {
                                Terminate("Use after free.");
                            }

                            std::unique_lock lock{this_->mutex};
                            this->pCallback = pCallback;

                            if (this_)
                            {
                                this_->AddAwaiter(this);
                            }

                            bool signaled = this->signaled;
                            if (!signaled)
                            {
                                if (this_->ready)
                                {
                                    signaled = true;
                                    this_->ready = false;
                                }
                                if (this->conditionTest != nullptr)
                                {
                                    try
                                    {
                                        signaled = this->conditionTest();
                                    }
                                    catch (const std::exception &e)
                                    {
                                        if (this_)
                                        {
                                            this_->RemoveAwaiter(this);
                                            this_ = nullptr;
                                        }
                                        auto t = pCallback;
                                        pCallback = nullptr;
                                        lock.unlock();
                                        t->SetException(std::current_exception());
                                        return;
                                    }
                                }
                            }
                            if (signaled)
                            {
                                if (this_)
                                {
                                    this_->RemoveAwaiter(this);
                                    this_ = nullptr;
                                }
                                auto t = pCallback;
                                pCallback = nullptr;
                                lock.unlock();

                                t->SetComplete();
                                return;
                            }
                            if (this->timeout != NO_TIMEOUT)
                            {
                                pCallback->RequestTimeout(this->timeout);
                            }
                        }
                    }

                    bool CancelExecute(CoServiceCallback<void> *pCallback)
                    {
                        if (closed)
                            return true;
                        if (this_)
                        {
                            std::lock_guard guard {this_->mutex};
                            bool result = this_->RemoveAwaiter(this);
                            this_ = nullptr;
                            return result;
                        } else {
                            return true;
                        }
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
    awaiters.push_back(awaiter);
}
bool CoConditionVariable::RemoveAwaiter(Awaiter *awaiter)
{
    CheckUseAfterFree();

    for (auto i = awaiters.begin(); i != awaiters.end(); ++i)
    {
        if ((*i) == awaiter)
        {
            awaiters.erase(i);
            awaiter->this_ = nullptr;
            return true;
        }
    }
    return false;
}

CoTask<> CoConditionVariable::Wait(
    std::chrono::milliseconds timeout,
    std::function<bool(void)> condition)
{
    CheckUseAfterFree();
    try {
        co_await detail::CoConditionaVariableImplementation::Wait_(this, condition, timeout);
    } catch (const std::exception &e)
    {
        throw;
    }
    co_return;
}

void CoConditionVariable::CheckUseAfterFree()
{
    // incomplete guard against use after free. But it catches lots.
    if (this->deleted)
    {
        Terminate("Use after free.");
    }
}
void CoConditionVariable::NotifyAll(function<void(void)> notifyAction)
{
    CheckUseAfterFree();
    // manage the queue under the mutex.
    std::vector<Awaiter *> readyAwaiters;
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
                try
                {
                    ready = awaiter->conditionTest();
                }
                catch (const std::exception &e)
                {
                    awaiter->UnhandledException();
                    ready = true;
                }
            }
            if (ready)
            {
                if (awaiter->pCallback != nullptr)
                {
                    readyAwaiters.push_back(awaiter);
                } else {
                    awaiter->signaled = true;
                }
                i = awaiters.erase(i);
                awaiter->this_ = nullptr;
            }
            else
            {
                ++i;
            }
        }
    }
    // do the callbacks without the mutex.
    for (auto i = readyAwaiters.begin(); i != readyAwaiters.end(); ++i)
    {
        Awaiter *pAwaiter = (*i);
        pAwaiter->SetComplete();
    }
}

void CoConditionVariable::Notify(std::function<void(void)> action)
{
    CheckUseAfterFree();

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
            try
            {
                ready = awaiter->conditionTest();
            }
            catch (const std::exception &e)
            {
                awaiters.erase(awaiters.begin());
                awaiter->this_ = nullptr;
                if (awaiter->pCallback != nullptr)
                {
                    auto t = awaiter->pCallback;
                    lock.unlock();
                    awaiter->pCallback = nullptr;
                    t->SetException(std::current_exception());
                    return;
                } else {
                    awaiter->signaled = true;
                }
            }
        }
        if (ready)
        {
            awaiters.erase(awaiters.begin());
            awaiter->this_ = nullptr;
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

CoConditionVariable::~CoConditionVariable()
{

    // manage the queue under the mutex.
    std::vector<CoServiceCallback<void>*> callbacks;
    {
        std::unique_lock lock{mutex};
        this->deleted = 0xBaadF00d; // no more access to this object.

        for (auto i = awaiters.begin(); i != awaiters.end(); i++)
        {
            auto awaiter = *i;
            awaiter->this_ = nullptr;
            awaiter->closed = true;
            if (awaiter->pCallback != nullptr)
            {
                try {
                    throw CoIoClosedException();
                } catch (const std::exception &)
                {
                    awaiter->UnhandledException();
                }
                if (awaiter->pCallback != nullptr)
                {
                    callbacks.push_back(awaiter->pCallback);
                    awaiter->pCallback = nullptr;
                }
            } else {
                awaiter->signaled = true;
            }
        }
        awaiters.resize(0); // callback is now irrevocably pending.
    }
    // do the callbacks without the mutex.
    for (auto i = callbacks.begin(); i != callbacks.end(); ++i)
    {
        CoServiceCallback<void> *pCallback = (*i);
        std::exception_ptr ptr;
        try {
            throw CoIoClosedException();
        } catch (const std::exception &)
        {
            ptr = std::current_exception();
        }
        pCallback->SetException(ptr);
    }
}