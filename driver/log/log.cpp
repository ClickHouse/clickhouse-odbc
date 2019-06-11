#include "log.h"

#include <chrono>
#include <thread>

bool log_enabled =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

std::string log_file = LOG_DEFAULT_FILE;
std::ofstream log_stream = log_enabled ? std::ofstream(log_file, std::ios::out | std::ios::app) : std::ofstream();
std::string log_header;

std::chrono::high_resolution_clock hr_clock;

std::ostream & log_prefix(std::ofstream & stream) {
    stream << std::chrono::duration_cast<std::chrono::microseconds>(hr_clock.now().time_since_epoch()).count() << " ["
           << std::this_thread::get_id() << "] ";

    return stream;
}
