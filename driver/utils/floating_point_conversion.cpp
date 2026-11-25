#include "driver/utils/type_info.h"

#include <cerrno>
#include <locale.h>
#include <memory>
#include <stdexcept>
#include <stdlib.h>
#include <type_traits>

namespace {

#if defined(_win_)
using NumericLocale = _locale_t;
constexpr auto parse_float = &_strtof_l;
constexpr auto parse_double = &_strtod_l;
#else
using NumericLocale = locale_t;
constexpr auto parse_float = &strtof_l;
constexpr auto parse_double = &strtod_l;
#endif

NumericLocale numericLocale() {
#if defined(_win_)
    static const std::unique_ptr<std::remove_pointer_t<NumericLocale>, decltype(&_free_locale)> locale{
        _create_locale(LC_ALL, "C"), &_free_locale};
#else
    static const std::unique_ptr<std::remove_pointer_t<NumericLocale>, decltype(&freelocale)> locale{
        newlocale(LC_ALL_MASK, "C", nullptr), &freelocale};
#endif
    if (!locale)
        throw std::runtime_error("Cannot create the C locale for floating-point conversion");
    return locale.get();
}

template <typename T, typename Parse>
void convertFloatingPoint(const std::string & src, T & dest, const char * type, Parse parse) {
    const auto locale = numericLocale();
    const char * start = src.c_str();
    char * end = nullptr;
    const int saved_errno = errno;
    errno = 0;
    const auto parsed = parse(start, &end, locale);
    const int parse_errno = errno;
    errno = saved_errno;

    if (end == start)
        throw std::runtime_error("Cannot interpret '" + src + "' as " + type + ": no conversion");
    if (parse_errno == ERANGE)
        throw std::runtime_error("Cannot interpret '" + src + "' as " + type + ": value out of range");

    dest = parsed;
    if (end != start + src.size())
        throw std::runtime_error("Cannot interpret '" + src + "' as " + type + ": string consumed partially");
}

}

namespace value_manip {

void from_value<std::string>::to_value<float>::convert(const SourceType & src, DestinationType & dest) {
    convertFloatingPoint(src, dest, "float", parse_float);
}

void from_value<std::string>::to_value<double>::convert(const SourceType & src, DestinationType & dest) {
    convertFloatingPoint(src, dest, "double", parse_double);
}

}
