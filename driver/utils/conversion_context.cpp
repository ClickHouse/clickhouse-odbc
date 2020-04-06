#include "driver/utils/conversion_context.h"

UnicodeConversionContext::UnicodeConversionContext(
    const std::string & application_wide_char_encoding,
    const std::string & application_narrow_char_encoding,
    const std::string & data_source_narrow_char_encoding,
    const std::string & driver_pivot_narrow_char_encoding
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
