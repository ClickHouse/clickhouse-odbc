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
    );

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
