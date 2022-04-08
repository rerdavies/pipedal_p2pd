

// Adress Sanitizer leak checks is broken on Raspberry Pi OS. Disable it.
const char* __asan_default_options() { 
    return "detect_leaks=0"; 
}