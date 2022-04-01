#include "cotask/CoTask.h"

using namespace cotask;

// minimal dispatcher create and shutdown
// used for valgrid debugging.



int main(int argc, char**argv)
{
    Dispatcher(); // create a CoDispatcher and thread pool

    Dispatcher().Log().SetLogLevel(Debug);

    Dispatcher().DestroyDispatcher();
    return 0;
}