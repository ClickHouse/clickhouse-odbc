#include "driver/utils/type_info.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace value_manip {

void from_value<std::string>::to_value<SQL_NUMERIC_STRUCT>::convert(
    const SourceType & src, DestinationType & dest
) {
    using Decimal = DataSourceType<DataSourceTypeId::Decimal>;
    const auto exponent_pos = src.find_first_of("eE");
    if (exponent_pos == std::string::npos) {
        convert_via_proxy<Decimal>(src, dest);
        return;
    }

    const auto mantissa = src.substr(0, exponent_pos);
    const char * begin = src.data() + exponent_pos + 1;
    const char * end = src.data() + src.size();
    const bool negative_exponent = begin != end && *begin == '-';
    if (begin != end && (*begin == '+' || negative_exponent))
        ++begin;
    std::uint32_t exponent = 0;
    const auto [ptr, ec] = std::from_chars(begin, end, exponent);
    if (ec == std::errc::result_out_of_range)
        throw SqlException("Scientific exponent is outside the supported numeric range", "22003");
    if (ec != std::errc() || ptr != end || mantissa.find_first_of("0123456789") == std::string::npos)
        throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric");

    Decimal decimal;
    using Scale = decltype(decimal.scale);
    if (mantissa.size() > static_cast<std::size_t>(std::numeric_limits<Scale>::max()))
        throw SqlException("Scientific mantissa is outside the supported numeric range", "22003");
    from_value<std::string>::to_value<Decimal>::convert(mantissa, decimal);

    // Applying the exponent moves the decimal point without changing the mantissa digits.
    const auto signed_exponent = negative_exponent ? -std::int64_t(exponent) : std::int64_t(exponent);
    const auto source_scale = std::int64_t(decimal.scale) - signed_exponent;
    if (source_scale < std::numeric_limits<Scale>::min() || source_scale > std::numeric_limits<Scale>::max())
        throw SqlException("Scientific exponent is outside the supported numeric range", "22003");
    decimal.scale = static_cast<Scale>(source_scale);

    if (dest.precision == 0) {
        const auto precision = std::max(decimal.precision, decimal.scale);
        if (precision > std::numeric_limits<SQLCHAR>::max()
            || source_scale < std::numeric_limits<SQLSCHAR>::min()
            || source_scale > std::numeric_limits<SQLSCHAR>::max())
            throw SqlException("Scientific value does not fit the numeric descriptor", "22003");
        decimal.precision = precision;
    }
    else {
        const auto first_significant = mantissa.find_first_of("123456789");
        const auto digits = first_significant == std::string::npos ? 0
            : std::int64_t(mantissa.size() - first_significant - (mantissa.find('.', first_significant) != std::string::npos));
        const auto scaled_digits = digits + dest.scale - source_scale;
        if (digits != 0 && scaled_digits > dest.precision)
            throw SqlException("Scientific value does not fit the numeric descriptor", "22003");
        if (digits == 0 || scaled_digits <= 0) {
            decimal.value = {};
            decimal.scale = dest.scale;
        }
    }

    from_value<Decimal>::to_value<DestinationType>::convert(decimal, dest);
}

}
