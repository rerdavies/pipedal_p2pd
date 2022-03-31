#include <bitset>

Task<> someTask(a,b,c)
StartThread(someTask(a,b,c))


    Task<>* t = new Task<>;

    *t = someTask(a,b,c);
    
    returns std::uniquePtr(Task<>());
    