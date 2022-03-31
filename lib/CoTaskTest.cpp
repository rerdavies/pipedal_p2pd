// See if we can get the simplest of coroutine examples to work on Gcc 10.

#include "p2psession/CoTask.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <exception>
#include <cassert>

using namespace p2psession;
using namespace std;

void terminate(const std::exception &e)
{
    cout << "ERROR: " << e.what() << endl;
    cout.flush();
    std::terminate();
}

/****** DelayTest ************************************/
CoTask<int> DelayTask2(int instance)
{
    cout << "    Enter Task2(instance)" << endl;
    co_await CoDelay(1000ms);
    cout << "Task2 Delay complete" << endl;
    cout << "    Exit Task2(instance)" << endl;
    co_return 1;
}

CoTask<int> DelayTask1()
{
    auto startMs = CoDispatcher::Now();
    int checkPoints = 1;

    cout << "Enter Task1" << endl;

    co_await CoDelay(1000ms);
    ++checkPoints;

    cout << "Task1 Delay complete" << endl;

    checkPoints += co_await DelayTask2(0);

    checkPoints += co_await DelayTask2(1);
    cout << "Exit Task1" << endl;

    auto elapsed = CoDispatcher::Now() - startMs;
    assert(elapsed.count() >= 3000);
    assert(checkPoints == 4);
    co_return checkPoints;
}

void DelayTest()
{

    cout << "--- DelayTest" << endl;
    CoTask<int> task = DelayTask1();

    task.GetResult();

    CoDispatcher::DestroyDispatcher();
    cout << "--- DelayTest Done" << endl;
}

void ConceptsTest()
{
    static_assert(Awaitable<CoTask<int>, int>);
    static_assert(Awaitable<CoTask<>, void>);
}

/***************************/
CoTask<int> BackgroundTask1()
{
    try
    {
        cout << "   background start" << endl;
        cout << "        IsForeground: " << CoDispatcher::CurrentDispatcher().IsForeground() << endl;
        cout << "   switch" << endl;

        assert(CoDispatcher::CurrentDispatcher().IsForeground());

        co_await CoBackground();
        cout << "        IsForeground: " << CoDispatcher::CurrentDispatcher().IsForeground() << endl;
        assert(!CoDispatcher::CurrentDispatcher().IsForeground());
        cout << "   switch" << endl;
        co_await CoForeground();
        cout << "        IsForeground: " << CoDispatcher::CurrentDispatcher().IsForeground() << endl;
        assert(CoDispatcher::CurrentDispatcher().IsForeground());

        cout << "   background end" << endl;
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        cout.flush();
        terminate(e);
    }
}

void BackgroundTest()
{
    cout << "--- BackgroundTest " << endl;
    CoTask<int> task = BackgroundTask1();

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();

    CoDispatcher::DestroyDispatcher();
    cout << "--- BackgroundTest  Done" << endl;

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();
    CoDispatcher::DestroyDispatcher(); // check for leasks
}

/***************************/
CoTask<int> BackgroundNested()
{
    cout << "Nest>" << endl;
    co_await BackgroundTask1();
    cout << "<Nest" << endl;
}

void BackgroundNestedTest()
{
    cout << "--- BackgroundNestedTest " << endl;
    CoTask<int> task = BackgroundNested();

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();

    CoDispatcher::DestroyDispatcher();
    cout << "--- BackgroundNestedTestDone" << endl;

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();
    CoDispatcher::DestroyDispatcher(); // check for leasks
}
/***************************/
CoTask<int> SwitchOnReturnBg()
{
    cout << "Nest>" << endl;
    co_await CoBackground();
    cout << "<Nest" << endl;
}
CoTask<int> SwitchOnReturnFg()
{
    cout << "Nest>" << endl;
    co_await CoForeground();
    cout << "<Nest" << endl;
}
CoTask<int> SwitchOnReturn()
{
    try
    {
        cout << ">" << endl;
        co_await SwitchOnReturnBg();
        cout << "        IsForeground: " << CoDispatcher::CurrentDispatcher().IsForeground() << endl;
        assert(!CoDispatcher::CurrentDispatcher().IsForeground());

        co_await SwitchOnReturnFg();
        cout << "        IsForeground: " << CoDispatcher::CurrentDispatcher().IsForeground() << endl;
        assert(CoDispatcher::CurrentDispatcher().IsForeground());
        cout << "<" << endl;
    }
    catch (const exception &e)
    {
        cout << "Error: " << e.what() << endl;
        cout.flush();
        terminate(e);
    }
}

void BackgroundSwitchOnreturnTest()
{
    cout << "--- BackgroundSwitchOnreturnTest " << endl;
    CoTask<int> task = SwitchOnReturn();
    task.GetResult();
    assert(CoDispatcher::CurrentDispatcher().IsDone());
    CoDispatcher::DestroyDispatcher();
    cout << "--- BackgroundSwitchOnreturnTest" << endl;
}
/**************************************/
using namespace std;

void TestThreadPoolSizing()
{
    CoDispatcher &dispatcher = CoDispatcher::CurrentDispatcher();

    dispatcher.SetThreadPoolSize(5);

    int threadPoolSize = CoDispatcher::Instrumentation::GetThreadPoolSize();
    assert(threadPoolSize == 5);
    dispatcher.SetThreadPoolSize(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100ms));

    int deadThreadCount = CoDispatcher::Instrumentation::GetNumberOfDeadThreads();

    CoDispatcher::CurrentDispatcher().PumpMessages(); // and for the dead threads to be reclaimed.

    threadPoolSize = CoDispatcher::Instrumentation::GetThreadPoolSize();
    assert(threadPoolSize == 1);
    deadThreadCount = CoDispatcher::Instrumentation::GetNumberOfDeadThreads();
    assert(deadThreadCount == 0);

    CoDispatcher::DestroyDispatcher();
}
/** VoidTest *************************************/

CoTask<> VoidTask1()
{
    co_await CoDelay(100ms);
}

CoTask<> VoidTask2()
{
    throw std::logic_error("Expected exception.");
}

void VoidTest()
{
    CoTask<> task = VoidTask1();
    task.GetResult();

    bool caught = false;
    try
    {
        CoTask<> task = VoidTask2();
        task.GetResult();
    }
    catch (std::exception &e)
    {
        caught = true;
        cout << "Expected exception: " << e.what() << endl;
    }
    assert(caught);
}
/***************************************/

CoTask<> CatchTest1()
{
    co_await CoDelay(300ms);
    cout << "          Throwing..." << endl;
    throw std::logic_error("Expected exception");
    co_return;
}

CoTask<std::string> CatchIntercept2()
{
    try
    {
        co_await CatchTest1();
    }
    catch (const std::exception &e)
    {
        co_return e.what();
    }
    co_return "No error thrown.";
}
CoTask<std::string> CatchPassThrough2()
{
    co_await CatchTest1();
    co_return "No error thrown.";
}

void CatchTest()
{
    cout << "------ CatchTest -----" << endl;
    // ensure that exceptions are propagated through a GetValue call.
    try
    {
        CoTask<std::string> task = CatchPassThrough2();
        std::string result = task.GetResult();
        assert (false && "Expecting an exception");

    }
    catch (const std::exception &e)
    {
        cout << "Expected exception: " << e.what() << endl;
    }


    try
    {
        CoTask<> task = CatchTest1();
        task.GetResult();
        assert(false && "Exception should have been thrown.");
    }
    catch (const std::exception &e)
    {
        cout << "Expected exception: " << e.what() << endl;
    }

    try
    {
        CoTask<std::string> task = CatchIntercept2();
        std::string result = task.GetResult();
        cout << "exception handled in coroutine: " << result << endl;
    }
    catch (const std::exception &e)
    {
        assert(false && "Exception was supposed to be caught.");
    }

    bool caught;
    
    std::exception_ptr eptr;
    try
    {
        throw std::invalid_argument("Expected error");
    }
    catch (const std::exception &e)
    {
        eptr = std::current_exception();
    }
    try
    {
        std::rethrow_exception(eptr);
    }
    catch (const std::exception &e)
    {
        std::string result = e.what();
        cout << " got it: " << result << endl;
        assert(result == std::string("Expected error"));
    }

    try
    {
        CoTask<std::string> task = CatchIntercept2();
        std::string result = task.GetResult();
        cout << "exception handled in coroutine: " << result << endl;
    }
    catch (const std::exception &e)
    {
        assert(false && "Should have been caught.");
        cout << "Expected exception: " << e.what() << endl;
    }
}
/***************************************/
int main(int argc, char **argv)
{
    CatchTest();
    // VoidTest();
    // TestThreadPoolSizing();
    // BackgroundSwitchOnreturnTest();
    // BackgroundNested();
    // BackgroundTest();
    // DelayTest();
    // ConceptsTest();

    Dispatcher().DestroyDispatcher();
    return 0;
}