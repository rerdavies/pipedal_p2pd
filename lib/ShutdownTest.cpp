#include "p2psession/CoTask.h"

using namespace p2psession;

// minimal dispatcher create and shutdown
// used for valgrid debugging.



int main(int argc, char**argv)
{
    Dispatcher(); // create a CoDispatcher and thread pool

    Dispatcher().Log().SetLogLevel(Debug);

    Dispatcher().DestroyDispatcher();
    return 0;
}