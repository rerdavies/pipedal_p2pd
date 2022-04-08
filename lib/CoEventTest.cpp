#include "cotask/CoEvent.h"
#include "cotask/CoExceptions.h"
#include "cotask/CoBlockingQueue.h"
#include "cotask/Os.h"
#include <cassert>

using namespace cotask;
using namespace std;
////// ConditionVariableTest ///////////////////

class CCoConditionVariableTest
{
public:
    int state = 0;

    int condVariable = 0;

    CoConditionVariable sourceConditionVariable;

    CoConditionVariable targetConditionVariable;

    CoTask<> Source()
    {

        co_await sourceConditionVariable.Wait();
        state = 1;
        targetConditionVariable.Notify();

        co_await sourceConditionVariable.Wait();

        if (!co_await sourceConditionVariable.Wait(250ms))
        {
            state = 3;
        }
        else
        {
            assert(0 && "Expecting a timeout.");
        }
        targetConditionVariable.Notify();

        co_await sourceConditionVariable.Wait();
        state = 5;

        co_await CoDelay(500ms); // get the target waiting.

        targetConditionVariable.Notify(
            [this] { this->condVariable = 1; });
        state = 6;
        targetConditionVariable.Notify(
            [this] { this->condVariable = 2; });
        state = 7;
        targetConditionVariable.Notify(
            [this] { this->condVariable = 3; });

        state = 8;
        targetConditionVariable.Notify(
            [this] { this->condVariable = 0; });

        co_return;
    }
    CoTask<> CoConditionVariableTest()
    {
        state = 0;
        Dispatcher().StartThread(Source());
        assert(state == 0);

        sourceConditionVariable.Notify();
        co_await targetConditionVariable.Wait();
        assert(state == 1);
        state = 2;
        sourceConditionVariable.Notify();
        co_await targetConditionVariable.Wait();

        // check for a timeout.
        // co_await CoDelay(10000ms);

        assert(state == 3);

        this->condVariable = 99;
        sourceConditionVariable.Notify();
        co_await targetConditionVariable.Wait([this] {
            return this->condVariable == 0;
        });
        cout << state << "," << condVariable << endl;
        assert(state == 8);

        Dispatcher().PostQuit();
    }
};
void ConditionVariableTest()
{
    cout << "--- Condition Variable Test ---" << endl;
    CCoConditionVariableTest test;
    Dispatcher().MessageLoop(test.CoConditionVariableTest());
    cout << "done" << endl;
}

//////////// MutexTest ////////////////////////

class CMutexTest
{
    static constexpr int instances = 10;
    static constexpr int locks_per_instance = 5;
    int counter = instances * locks_per_instance;
    CoMutex mutex;

    [[nodiscard]] CoTask<> MutexTask(int id)
    {
        for (int i = 0; i < locks_per_instance; ++i)
        {
            co_await mutex.CoLock();
            cout << "        Thread " << id << "," << i << endl;
            --counter;
            mutex.Unlock();
        }
        co_return;
    }
    [[nodiscard]] CoTask<> CoMain(bool foreground)
    {
        if (foreground)
        {
            cout << "    Foreground" << endl;
            co_await CoForeground();
        }
        else
        {
            cout << "    Background" << endl;
            co_await CoBackground();
        }
        co_await mutex.CoLock();
        counter = instances * locks_per_instance;
        for (int i = 1; i <= instances; ++i)
        {
            Dispatcher().StartThread(MutexTask(i));
        }
        mutex.Unlock();
        bool done = false;
        while (!done)
        {
            co_await CoDelay(100ms);
            co_await mutex.CoLock();
            if (counter == 0)
            {
                done = true;
            }
            mutex.Unlock();
        }
        Dispatcher().PostQuit();
    }

public:
    void Run()
    {
        CoDispatcher &dispatcher = Dispatcher();
        dispatcher.MessageLoop(CoMain(true));

        dispatcher.MessageLoop(CoMain(false));
    }
};

void MutexTest()
{
    CMutexTest test;
    test.Run();
}
//////////// Blocking queue test //////////////

int leakCount = 0;
class TestTarget
{
public:
    TestTarget() { ++leakCount; }
    ~TestTarget() { --leakCount; }

    int i;
};

class CBlockingQueueTest
{
public:
    using bq_element_type = TestTarget;

    using queue_type = CoBlockingQueue<bq_element_type>;

    CoTask<> Writer(bool foreground, queue_type &queue, int numberOfWrites, int writeBatchSize)
    {
        if (foreground)
        {
            co_await CoForeground();
        }
        else
        {
            co_await CoBackground();
        }

        for (int i = 0; i < numberOfWrites; ++i)
        {
            TestTarget *value = new TestTarget();
            if (i % writeBatchSize == 0)
            {
                co_await CoDelay(111ms);
            }
            co_await queue.Push(value);
        }
        queue.Close();
    }
    CoTask<> Reader(queue_type &queue, int numberOfWrites, int readerBatchSize)
    {
        int numberOfReads = 0;
        try
        {
            while (true)
            {
                queue_type::item_type *value = co_await queue.Take();
                delete value;
                ++numberOfReads;
                if (numberOfReads % readerBatchSize)
                {
                    co_await CoDelay(87ms);
                }
            }
        }
        catch (const CoQueueClosedException &e)
        {
            cout << "        Got QueueClosed exception." << endl;
        }
        assert(numberOfReads == numberOfWrites);
        co_return;
    }

    CoTask<> TestBlockingQueue(bool foreground, int numberOfWrites, size_t queueSize, int readerBatchSize, int writeBatchSize)
    {
        if (foreground)
        {
            co_await CoForeground();
        }
        else
        {
            co_await CoBackground();
        }

        queue_type queue{queueSize};
        Dispatcher().StartThread(Writer(foreground, queue, numberOfWrites, writeBatchSize));

        co_await Reader(queue, numberOfWrites, readerBatchSize);

        assert(leakCount == 0);
    }

    void Run()
    {
        cout << "--- TestBlockingQueue" << endl;
        cout << "    Foreground" << endl;
        CoTask<> task = TestBlockingQueue(true, 100, 6, 7, 8);
        task.GetResult();
        cout << "    Background" << endl;
        task = TestBlockingQueue(false, 99, 5, 11, 7);
        task.GetResult();
        cout << "    Done" << endl;
    }
};

void BlockingQueueTest()
{
    CBlockingQueueTest test;
    test.Run();
}

///////////////////////////////////////////////

int main(int argc, char **argv)
{
    BlockingQueueTest();

    MutexTest();
    ConditionVariableTest();
    Dispatcher().DestroyDispatcher();

    return 0;
}