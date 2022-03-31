// See if we can get the simplest of coroutine examples to work on Gcc 10.

#include "p2psession/CoTask.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <exception>

using namespace p2psession;
using namespace std;

void assert(bool condition)
{
    if (!condition)
    {
        throw std::logic_error("Assertion failed.");
    }
}
void assert(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::logic_error("Assertion failed:" + message);
    }
}
void terminate(const std::exception &e)
{
    cout << "ERROR: " << e.what() << endl;
    cout.flush();
    std::terminate();    
}


/****** DelayTest ************************************/
Task<int> DelayTask2(int instance)
{
    cout << "    Enter Task2(instance)" << endl;
    co_await CoDelay(1000ms);
    cout << "Task2 Delay complete" << endl;
    cout << "    Exit Task2(instance)" << endl;
    co_return 1;
}

Task<int> DelayTask1()
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
    Task<int> task = DelayTask1();

    task.GetResult();

    CoDispatcher::DestroyDispatcher();
    cout << "--- DelayTest Done" << endl;
}

void ConceptsTest()
{
    static_assert(Awaitable<Task<int>, int>);
    static_assert(Awaitable<Task<>, void>);
}

/***************************/
Task<int> BackgroundTask1()
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
    Task<int> task = BackgroundTask1();

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();

    CoDispatcher::DestroyDispatcher();
    cout << "--- BackgroundTest  Done" << endl;

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();
    CoDispatcher::DestroyDispatcher(); // check for leasks
}

/***************************/
Task<int> BackgroundNested()
{
    cout << "Nest>" << endl;
    co_await BackgroundTask1();
    cout << "<Nest" << endl;
}

void BackgroundNestedTest()
{
    cout << "--- BackgroundNestedTest " << endl;
    Task<int> task = BackgroundNested();

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();

    CoDispatcher::DestroyDispatcher();
    cout << "--- BackgroundNestedTestDone" << endl;

    CoDispatcher::CurrentDispatcher().PumpUntilIdle();
    CoDispatcher::DestroyDispatcher(); // check for leasks
}
/***************************/
Task<int> SwitchOnReturnBg()
{
    cout << "Nest>" << endl;
    co_await CoBackground();
    cout << "<Nest" << endl;
}
Task<int> SwitchOnReturnFg()
{
    cout << "Nest>" << endl;
    co_await CoForeground();
    cout << "<Nest" << endl;
}
Task<int> SwitchOnReturn()
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
    Task<int> task = SwitchOnReturn();
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

Task<> VoidTask1()
{
    co_await CoDelay(100ms);
}

Task<> VoidTask2()
{
    throw std::logic_error("Expected exception.");
}


void VoidTest()
{
    Task<> task = VoidTask1();
    task.GetResult();

    bool caught = false;
    try {
        Task<> task = VoidTask2();
        task.GetResult();
    } catch (std::exception &e)
    {
        caught = true;
        cout << "Expected exception: " << e.what() << endl;
    }
    assert(caught);


}
/***************************************/

Task<> CatchTest1()
{
    co_await CoDelay(200ms);
    throw std::logic_error("Expected exception"); 
    co_return;
}

Task<std::string> CatchTest2()
{
    try {
        co_await CatchTest1();
    } catch (const std::exception &e)
    {
        co_return e.what();

    }
    co_return "No error thrown.";
}

void CatchTest()
{
    // ensure that exceptions are propagated through a GetValue call.

    // This one terminates. It's understandable, given that we're asking GCC to propagate an 
    // exception under difficult circumstances.
    try {
        Task<> task = CatchTest1();
        task.GetResult();
    } catch (const std::exception &e)
    {
        cout << "Expected exception: " << e.what() << endl;
    }
    std::exception_ptr eptr;
    try {
        throw std::invalid_argument("Expected error");
    } catch (const std::exception & e)
    {
        eptr = std::current_exception();
    }
    try {
        std::rethrow_exception(eptr);
    } catch (const std::exception & e)
    {
        std::string result = e.what();
        cout << " got it: " << result << endl;
        assert(result == std::string("Expected error"));
    }

    try {
        Task<std::string> task = CatchTest2();
        std::string result = task.GetResult();
        cout << "exception handled in coroutine: " << result << endl;

    } catch (const std::exception &e)
    {
        cout << "Expected exception: " << e.what() << endl;
    }
}
/***************************************/
int main(int argc, char **argv)
{
    CatchTest();
    VoidTest();
    TestThreadPoolSizing();
    BackgroundSwitchOnreturnTest();
    BackgroundNested();
    BackgroundTest();
    DelayTest();
    ConceptsTest();
}