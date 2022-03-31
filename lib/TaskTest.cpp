#include "p2psession/CoTask.h"

using namespace p2psession;
using namespace std;

CoTask<> TestTask2()
{
    co_return;
}


CoTask<> TestTask()
{
    co_await TestTask2();
    Dispatcher().PostQuit();
    co_return;
}


int main(int argc, char**argv)
{
    Dispatcher().MessageLoop(TestTask());

    Dispatcher().DestroyDispatcher();
}