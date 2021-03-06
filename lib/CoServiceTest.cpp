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

#include "cotask/CoTask.h"
#include "cotask/CoService.h"
#include <cassert>
#include <chrono>

using namespace cotask;
using namespace std;


//**********************************

auto SimpleService(
    chrono::milliseconds execTimeMs = 500ms, 
    chrono::milliseconds timeoutMs = -1ms,
    bool delayCancelExecute = false) {
    // Capture argumens.
    struct Args { 

        using return_type = bool;
        bool dummy;
        chrono::milliseconds timeoutMs;
        chrono::milliseconds execTimeMs;
        bool delayCancelExecute;

        uint64_t execTimer;

        CoServiceCallback<return_type> *pCallback = nullptr;
        void Execute(CoServiceCallback<return_type> *pCallback)
        {
            this->pCallback = pCallback;
            if (timeoutMs != -1ms)
            {
                pCallback->RequestTimeout(timeoutMs);
            }
            if (execTimeMs ==  0ms)
            {
                 pCallback->SetResult(true);
            } else {
                execTimer = CoDispatcher::CurrentDispatcher().PostDelayedFunction(
                    execTimeMs,
                    [this] () {
                        this->pCallback->SetResult(true);
                    });
            }
        }
        bool CancelExecute(CoServiceCallback<return_type> *pCallback)
        {
            
            if (delayCancelExecute)
            {
                return false;
            } else {
                pCallback = nullptr;
                return CoDispatcher::CurrentDispatcher().CancelDelayedFunction(execTimer);
            }
        }

    };

    static_assert(IsTypedCoServiceImplementation<Args>);
    using Awaitable = CoService<Args>;

    Awaitable awaitable {};
    awaitable.dummy = true;
    awaitable.execTimeMs = execTimeMs;
    awaitable.timeoutMs = timeoutMs;
    awaitable.delayCancelExecute = delayCancelExecute;
    return awaitable;
}

// GCC 10.2 gives spurious warnings in coroutines.
#pragma GCC diagnostic ignored "-Wunused-but-set-variable" 


CoTask<> SimpleServiceTest1(bool onForeground) {
    if (onForeground)
    {
        co_await CoForeground();
    } else {
        co_await CoBackground();
    }


    cout << "    Waiting (resume in suspend call)." << endl;
    co_await SimpleService(0ms);
    assert(CoDispatcher::CurrentDispatcher().IsForeground() == onForeground);

    cout << "    Waiting 1s." << endl;
    co_await SimpleService(1000ms);
    assert(CoDispatcher::CurrentDispatcher().IsForeground() == onForeground);

    cout << "    Waiting 1 s." << endl;
    co_await SimpleService(1000ms);
    assert(CoDispatcher::CurrentDispatcher().IsForeground() == onForeground);

    bool timedOut;
    
    cout << "    Waiting 1s. w/o timeout" << endl;
    timedOut = false;
    try {
        co_await(SimpleService(1000ms,200000ms));
    } catch (const CoTimedOutException &e)
    {
        timedOut = true;
    }
    assert(!timedOut);

    cout << "    Waiting 1s. w/ timeout" << endl;
    timedOut = false;
    try {
        co_await(SimpleService(200000ms,1000ms));
    } catch (const CoTimedOutException &e)
    {
        timedOut = true;
    }
    assert(timedOut);

    cout << "    Waiting 1s (0ms timeout -> no timeout.)" << endl;
    timedOut = false;
    try {
        co_await(SimpleService(1000ms,0ms));
    } catch (const CoTimedOutException &e)
    {
        timedOut = true;
    }
    assert(!timedOut);

    cout << "    Waiting 1s w/ immediate timeout (Executing state -> ExecutingTimedOut)" << endl;
    timedOut = false;
    try {
        co_await(SimpleService(1000ms,-2ms));
    } catch (const CoTimedOutException &e)
    {
        timedOut = true;
    }
    assert(timedOut);

    cout << "    Waiting 1s /w Timeout in flight (TimedOut->TimedOutExecuteInFlight -> Resuming)" << endl;
    timedOut = false;
    try {
        co_await(SimpleService(1000ms,500ms,true));
    } catch (const CoTimedOutException &e)
    {
        timedOut = true;
    }
    assert(!timedOut);

    co_return;
}

CoTask<> SimpleServiceTest()
{
    cout << "    On Foreground:" << endl;
    co_await SimpleServiceTest1(true);

    cout << "    On Background:" << endl;
    co_await SimpleServiceTest1(false);


    co_await CoForeground();

    co_return;
}


void TestSimpleService()
{
    cout << "---- TestSimpleService ---" << endl;
    
    CoTask<> task = SimpleServiceTest();

    task.GetResult();
}

//**********************************

int main(int argc, char**argv)
{
    // TestServiceTimeout();
    TestSimpleService();

    Dispatcher().DestroyDispatcher();
    return 0;
}