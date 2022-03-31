#include "p2psession/WpaCliWrapper.h"


using namespace p2psession;


int main(int argc, char**argv)
{
    CoDispatcher&dispatcher = p2psession::Dispatcher();
    WpaCliWrapper wpaCli;

    dispatcher.MessageLoop(wpaCli.Run(argc,argv));


    return 0;
}