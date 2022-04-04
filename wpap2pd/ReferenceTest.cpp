#include <functional>
#include <cassert>
#include <iostream>
#include <list>
#include <memory>

using namespace std;

static int constructs = 0;
static int plainConstructs = 0;
static int destructs = 0;
static int movedDestructs = 0;
static int copyConstructs = 0;
static int moveConstructs = 0;
static int moveAssigns = 0;
static int copyAssigns = 0;
static int gRefCheck = 0;

void ResetCounts()
{
    constructs = 0;
    plainConstructs = 0;
    destructs = 0;
    movedDestructs = 0;
    copyConstructs = 0;
    moveConstructs = 0;
    moveAssigns = 0;
    copyAssigns = 0;
    gRefCheck = 0;
}
class TestTarget
{
public:
    TestTarget()
    {
        ++gRefCheck;
        ++refCheck;
        ++plainConstructs;
        ++constructs;
    }
    TestTarget(const TestTarget &other)
    {
        ++gRefCheck;
        ++copyConstructs;
        ++constructs;
        ++refCheck;
    }

    TestTarget(TestTarget &&other)
    {
        ++gRefCheck;
        ++moveConstructs;
        other.moved = true;
        ++constructs;
        ++refCheck;
    }
    bool IsDangling() const
    {
        return refCheck == 0 || valid != 0;
    }
    TestTarget &operator=(const TestTarget &other)
    {
        ++copyAssigns;
        return *this;
    }
    TestTarget &operator=(TestTarget &&other)
    {
        other.moved = true;
        ++moveAssigns;
        return *this;
    }
    ~TestTarget()
    {
        if (moved)
        {
            ++movedDestructs;
        }
        --gRefCheck;
        --refCheck;
        ++destructs;
        assert(refCheck == 0);
    }

    int valid = 0;
    bool moved = false;
    int refCheck = 0;
};

template <typename T>
class CLambdaTest
{
public:
    function<void(void)> ConstRefCapture(const T &value)
    {
        function<void(void)> fnConstRef = [this, value]() {
            assert(!value.IsDangling());
        };
        return fnConstRef;
    }
    function<void(void)> MoveCapture(T &&value)
    {
        function<void(void)> fnMove = [this, value]() {
            assert(!value.IsDangling());
        };
        return fnMove;
    }

    function<void(void)> ExplicitMoveCapture(T &&value)
    {
        function<void(void)> fnMove = [this, valueCopy = std::move(value)]() {
            assert(!valueCopy.IsDangling());
        };
        return fnMove;
    }
};

void Report()
{
    cout << "Constructs: " << constructs << " Plain: " << plainConstructs << " Move: " << moveConstructs << " Copy: " << copyConstructs << endl;
    cout << "Destructs: " << destructs << " Moved destructs:" << movedDestructs << endl;
    cout << "Move assigns: " << moveAssigns << " Copy assigns: " << copyAssigns << endl;
    cout << "gRefCheck: " << gRefCheck << endl;
}
void TestConstRef()
{
    cout << "--- const T& ---- " << endl;
    ResetCounts();
    {
        CLambdaTest<TestTarget> test;
        std::function<void(void)> fnConstRef;
        {
            TestTarget t;
            fnConstRef = test.ConstRefCapture(t);
            cout << "Copies: " << copyConstructs  << " Moves: " << moveConstructs << endl;
        }

        fnConstRef();
    }
    Report();
    assert(constructs == destructs);
}
void TestMove()
{
    cout << "--- T&& [this,value]---- " << endl;
    ResetCounts();
    {
        CLambdaTest<TestTarget> test;
        std::function<void(void)> fnMove;
        {
            TestTarget t;
            fnMove = test.MoveCapture(std::move(t));

            Report();
            cout << endl;

        }

        fnMove();
    }
    Report();

    assert(constructs == destructs);
}

void TestExplicitMove()
{
    cout << "--- T&& [valueCopy=std::move(value)] ---- " << endl;
    ResetCounts();
    {
        CLambdaTest<TestTarget> test;
        std::function<void(void)> fnMove;
        {
            TestTarget t;
            fnMove = test.ExplicitMoveCapture(std::move(t));

            cout << "Copies: " << copyConstructs  << " Moves: " << moveConstructs << endl;

        }
        Report();
        cout << endl;
        fnMove();

        Report();
        cout << endl;

        fnMove();

    }
    Report();

    assert(constructs == destructs);
}


void QueueTest()
{
    cout << "--- list test --- " << endl;

    ResetCounts();

    using list_type = std::list<std::shared_ptr<TestTarget>>;
    
    list_type list;

    std::shared_ptr<TestTarget> item = std::make_shared<TestTarget>();

    list.push_back(std::move(item));

    assert(!item);

    std::shared_ptr<TestTarget> out = list.front();

    cout << "Out use count: " << out.use_count() << endl;
    list.pop_front();
    cout << "Out use count: " << out.use_count() << endl;
    out = nullptr;

    Report();


    Report();
}

int main(int, char **)
{

    QueueTest();
    TestConstRef();
    TestMove();
    TestExplicitMove();
    return 0;
}
