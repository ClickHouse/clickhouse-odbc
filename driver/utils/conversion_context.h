#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/string_pool.h"
#include "driver/utils/unicode_converter.h"

using ApplicationWideCharType = SQLWCHAR;
using ApplicationNarrowCharType = SQLCHAR;
using DataSourceNarrowCharType = char;
using DriverPivotNarrowCharType = char;

class UnicodeConversionContext {
public:
    explicit UnicodeConversionContext(
#if defined(_win_)
        const std::string & application_wide_char_encoding    = (sizeof(ApplicationWideCharType) > 2 ? "UTF-32" : "UTF-16"),
#else
        const std::string & application_wide_char_encoding    = (sizeof(ApplicationWideCharType) > 2 ? "UCS-4" : "UCS-2"),
#endif
        const std::string & application_narrow_char_encoding  = "UTF-8",
        const std::string & data_source_narrow_char_encoding  = "UTF-8",
        const std::string & driver_pivot_narrow_char_encoding = "UTF-8"
    )
        : application_wide_char_converter    {application_wide_char_encoding}
        , application_narrow_char_converter  {application_narrow_char_encoding}
        , data_source_narrow_char_converter  {data_source_narrow_char_encoding}
        , driver_pivot_narrow_char_converter {driver_pivot_narrow_char_encoding}

        , skip_application_to_converter_pivot_wide_char_conversion (sameEncoding(application_wide_char_encoding, converter_pivot_wide_char_encoding))
        , skip_application_to_driver_pivot_narrow_char_conversion  (sameEncoding(application_narrow_char_encoding, driver_pivot_narrow_char_encoding))
        , skip_data_source_to_driver_pivot_narrow_char_conversion  (sameEncoding(data_source_narrow_char_encoding, driver_pivot_narrow_char_encoding))
    {
        if (sizeof(ApplicationWideCharType) != application_wide_char_converter.getEncodedMinCharSize())
            throw std::runtime_error("unsuitable character type for the application wide-char encoding");

        if (sizeof(ApplicationNarrowCharType) != application_narrow_char_converter.getEncodedMinCharSize())
            throw std::runtime_error("unsuitable character type for the application narrow-char encoding");

        if (sizeof(DataSourceNarrowCharType) != data_source_narrow_char_converter.getEncodedMinCharSize())
            throw std::runtime_error("unsuitable character type for the data source narrow-char encoding");

        if (sizeof(DriverPivotNarrowCharType) != driver_pivot_narrow_char_converter.getEncodedMinCharSize())
            throw std::runtime_error("unsuitable character type for the driver pivot narrow-char encoding");
    }

public:
    StringPool string_pool{10};

    UnicodeConverter application_wide_char_converter;
    UnicodeConverter application_narrow_char_converter;
    UnicodeConverter data_source_narrow_char_converter;
    UnicodeConverter driver_pivot_narrow_char_converter;

    const bool skip_application_to_converter_pivot_wide_char_conversion = false;
    const bool skip_application_to_driver_pivot_narrow_char_conversion  = false;
    const bool skip_data_source_to_driver_pivot_narrow_char_conversion  = false;
};

// In future, this will become an aggregate context that will do proper date/time, etc., conversions also.
using DefaultConversionContext = UnicodeConversionContext;
