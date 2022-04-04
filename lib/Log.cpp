#include "cotask/Log.h"


using namespace cotask;


// strftime format
#define LOGGER_PRETTY_TIME_FORMAT "%Y-%m-%d %H:%M:%S"

// printf format
#define LOGGER_PRETTY_MS_FORMAT ".%03d"

// convert current time to milliseconds since unix epoch
template <typename T>
static auto to_ms(const std::chrono::time_point<T>& tp)
{
    using namespace std::chrono;

    auto dur = tp.time_since_epoch();
    return (duration_cast<milliseconds>(dur).count());
}


std::string ConsoleLog::now()
{
    auto tp = std::chrono::system_clock::now();
    std::time_t current_time = std::chrono::system_clock::to_time_t(tp);

    // must be protected by mutex!
    std::tm* time_info = std::localtime(&current_time);

    char buffer[128];

    int string_size = strftime(
        buffer, sizeof(buffer),
        LOGGER_PRETTY_TIME_FORMAT,
        time_info
    );

    int ms = to_ms(tp) % 1000;

    string_size += std::snprintf(
        buffer + string_size, sizeof(buffer) - string_size,
        LOGGER_PRETTY_MS_FORMAT, ms
    );

    return std::string(buffer, buffer + string_size);
}