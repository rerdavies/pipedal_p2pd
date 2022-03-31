#include <bitset>

CoTask<> someTask(a,b,c)
StartThread(someTask(a,b,c))


    CoTask<>* t = new CoTask<>;

    *t = someTask(a,b,c);
    
    returns std::uniquePtr(CoTask<>());
    