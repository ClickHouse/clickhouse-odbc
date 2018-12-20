#include "log.h"

bool log_enabled =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

std::string log_file = LOG_DEFAULT_FILE;
std::ofstream log_stream(log_file, std::ios::out | std::ios::app);
