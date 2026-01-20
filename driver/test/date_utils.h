#include "driver/test/client_test_base.h"

inline bool operator==(const SQL_TIMESTAMP_STRUCT & lhs, const SQL_TIMESTAMP_STRUCT & rhs) {
    return lhs.year == rhs.year
        && lhs.month == rhs.month
        && lhs.day == rhs.day
        && lhs.hour == rhs.hour
        && lhs.minute == rhs.minute
        && lhs.second == rhs.second
        && lhs.fraction == rhs.fraction;
}

inline bool operator==(const SQL_DATE_STRUCT & lhs, const SQL_DATE_STRUCT & rhs) {
    return lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day;
}

inline bool operator==(const SQL_TIME_STRUCT & lhs, const SQL_TIME_STRUCT & rhs) {
    return lhs.hour == rhs.hour && lhs.minute == rhs.minute && lhs.second == rhs.second;
}

inline void PrintTo(const SQL_TIMESTAMP_STRUCT & ts, std::ostream * os) {
    *os << ts.year << "-"
        << std::setfill('0') << std::setw(2) << ts.month << "-"
        << std::setfill('0') << std::setw(2) << ts.day << " "
        << std::setfill('0') << std::setw(2) << ts.hour << ":"
        << std::setfill('0') << std::setw(2) << ts.minute << ":"
        << std::setfill('0') << std::setw(2) << ts.second;
    if (ts.fraction > 0) {
        *os << "." << ts.fraction;
    }
}

inline void PrintTo(const SQL_DATE_STRUCT & d, std::ostream * os) {
    *os << d.year << "-"
        << std::setfill('0') << std::setw(2) << d.month << "-"
        << std::setfill('0') << std::setw(2) << d.day;
}

inline void PrintTo(const SQL_TIME_STRUCT & t, std::ostream * os) {
    *os << std::setfill('0') << std::setw(2) << t.hour << ":"
        << std::setfill('0') << std::setw(2) << t.minute << ":"
        << std::setfill('0') << std::setw(2) << t.second;
}
