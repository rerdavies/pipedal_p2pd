#include <iostream>
using namespace std;


// Adress Sanitizer leak checks is broken on Raspberry Pi OS. Disable it.
const char* __asan_default_options() { 
    cout << "ADDRESS_SANIZTER_DEFAULT_OPTIONS" << endl;
    return "detect_leaks=0"; 
}