#include "log.h"

bool log_enabled =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

std::ofstream logstream(LOG_DEFAULT_FILE, std::ios::out | std::ios::app);

