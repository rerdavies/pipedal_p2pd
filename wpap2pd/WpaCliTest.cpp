#include "includes/WpaSupplicant.h"


using namespace p2p;
using namespace cotask;


int main(int argc, char**argv)
{
    CoDispatcher&dispatcher = cotask::Dispatcher();
    WpaSupplicant wpaCli;

    dispatcher.MessageLoop(wpaCli.Run(argc,argv));

    return 0;
}