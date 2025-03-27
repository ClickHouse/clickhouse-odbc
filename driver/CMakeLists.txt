function (declare_odbc_lib_targets libname UNICODE)

# See the usage of each workaround in the code, for comments.
unset (WORKAROUND_ALLOW_UNSAFE_DISPATCH)
unset (WORKAROUND_DISABLE_SSL)
unset (WORKAROUND_USE_ICU)
unset (WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
unset (WORKAROUND_ENABLE_DEFINE_SQLBindParam)
unset (WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS)

if (UNICODE)
    set (DRIVER_PREFIX Unicode)
    set (WIDECHAR_SUFFIX w)
else ()
    set (DRIVER_PREFIX ANSI)
    set (WIDECHAR_SUFFIX)
    if ("${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        set (WORKAROUND_ENABLE_TRIM_TRAILING_NULL 1)
    endif ()
endif ()

if ("${ODBC_PROVIDER}" STREQUAL "iODBC")
#   set (WORKAROUND_ENABLE_DEFINE_SQLBindParam 1)
    set (WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS 1)
endif ()

if (CH_ODBC_ALLOW_UNSAFE_DISPATCH)
    set (WORKAROUND_ALLOW_UNSAFE_DISPATCH 1)
endif()

if (NOT CH_ODBC_ENABLE_SSL)
    set (WORKAROUND_DISABLE_SSL 1)
endif()

if (CH_ODBC_USE_ICU)
    set (WORKAROUND_USE_ICU 1)
endif ()

message (STATUS "${libname}: ${DRIVER_PREFIX}")

configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/config_cmake.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/platform/config_cmake${WIDECHAR_SUFFIX}.h
)

unset (WORKAROUND_ALLOW_UNSAFE_DISPATCH)
unset (WORKAROUND_DISABLE_SSL)
unset (WORKAROUND_USE_ICU)
unset (WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
unset (WORKAROUND_ENABLE_DEFINE_SQLBindParam)
unset (WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS)

# In order to enable testing, put every non-public symbol to a static library (which is then used by shared library and unit-test binary).
add_library (${libname}-impl STATIC
    utils/type_parser.cpp
    utils/type_info.cpp
    utils/unicode_converter.cpp
    utils/conversion_context.cpp

    config/config.cpp

    escaping/escape_sequences.cpp
    escaping/lexer.cpp

    format/ODBCDriver2.cpp
    format/RowBinaryWithNamesAndTypes.cpp

    api/impl/impl.cpp

    attributes.cpp
    connection.cpp
    descriptor.cpp
    diagnostics.cpp
    driver.cpp
    environment.cpp
    exception.cpp
    object.cpp
    result_set.cpp
    statement.cpp


    ${CMAKE_CURRENT_BINARY_DIR}/platform/config_cmake${WIDECHAR_SUFFIX}.h
    platform/platform.h

    utils/utils.h
    utils/iostream_debug_helpers.h
    utils/amortized_istream_reader.h
    utils/resize_without_initialization.h
    utils/object_pool.h
    utils/string_pool.h
    utils/unicode_converter.h
    utils/conversion_context.h
    utils/conversion.h
    utils/conversion_std.h
    utils/conversion_icu.h
    utils/type_parser.h
    utils/type_info.h

    config/config.h
    config/ini_defines.h

    escaping/escape_sequences.h
    escaping/lexer.h
    escaping/lexer_declare.h
    escaping/function_declare.h

    api/impl/impl.h

    format/ODBCDriver2.h
    format/RowBinaryWithNamesAndTypes.h

    attributes.h
    connection.h
    descriptor.h
    diagnostics.h
    driver.h
    environment.h
    exception.h
    object.h
    result_set.h
    statement.h
)

set (WIN_SOURCES)
if (WIN32)
    set (WIN_SOURCES
        platform/win/dllmain.cpp
        platform/win/resource.h
        platform/win/setup.cpp
        platform/win/resource.rc
    )

    if (NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        # An entry in .def file still must be added for each exported function for 32-bit targets.
        # See also the definition of 'EXPORTED_FUNCTION' C/C++ macro in the code.
        set (WIN_SOURCES
            ${WIN_SOURCES}
            platform/win/${libname}.def
        )
    endif ()
endif ()

if (UNICODE)
    target_compile_definitions (${libname}-impl
        PUBLIC UNICODE
        PUBLIC _UNICODE
    )
endif ()

target_include_directories (${libname}-impl
    PUBLIC ${FOLLY_INLUDE_DIR}      # TODO: switch to linking with folly lib target?
    PUBLIC ${PROJECT_BINARY_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}
)

if (CH_ODBC_ENABLE_SSL)
    if (MSVC)
        set (_poco_net Poco::NetSSLWin)
    else ()
        set (_poco_net Poco::Net Poco::Net::SSL Poco::Util)
    endif ()
else ()
    set (_poco_net Poco::Net)
endif ()

target_link_libraries (${libname}-impl
    PUBLIC Poco::Net
    PUBLIC Poco::Util
    PUBLIC Poco::Foundation
    PUBLIC Threads::Threads
)
if (OS_LINUX OR OS_DARWIN)
    target_link_libraries (${libname}-impl
        PUBLIC OpenSSL::SSL
        PUBLIC OpenSSL::Crypto
        PUBLIC Poco::Net::SSL
    )
elseif(OS_WINDOWS)
    target_link_libraries (${libname}-impl
        PUBLIC Poco::Net::WinSSL
    )
endif()

if (OS_LINUX OR OS_DARWIN)
    target_link_libraries (${libname}-impl
        PRIVATE ch_contrib::unixodbc
        PRIVATE ch_contrib::nanodbc
    )
elseif (OS_WINDOWS)
    target_link_libraries (${libname}-impl
        PRIVATE ODBC::Driver
    )
endif()


if (CH_ODBC_USE_ICU)
    target_link_libraries (${libname}-impl
        PUBLIC ch_contrib::icu
    )
endif ()

add_library (${libname} SHARED
    api/odbc.cpp
    api/sql_columns_resultset_mutator.cpp
    ${WIN_SOURCES}
)

target_link_libraries (${libname}
    PRIVATE ${libname}-impl
)

set_target_properties (${libname} PROPERTIES
    OUTPUT_NAME "clickhouseodbc${WIDECHAR_SUFFIX}"
)

if (OS_LINUX)
    set_target_properties(${libname} PROPERTIES
        LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exported_symbols_linux.txt"
    )
elseif (OS_DARWIN)
    #set_target_properties(${libname} PROPERTIES
    #    LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/exported_symbols_darwin.txt"
    #)
endif()


if (CH_ODBC_ENABLE_INSTALL)
    if (MSVC)
        install (
            TARGETS ${libname}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT "${DRIVER_PREFIX}Driver"
        )
        install (
            FILES $<TARGET_PDB_FILE:${libname}>
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT "${DRIVER_PREFIX}DriverDebugSymbols"
            OPTIONAL
        )
    else ()
        install (
            TARGETS ${libname}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT "${DRIVER_PREFIX}Driver"
        )
    endif ()
endif ()

endfunction ()

declare_odbc_lib_targets (clickhouse-odbc 0)
declare_odbc_lib_targets (clickhouse-odbcw 1)

if (CH_ODBC_ENABLE_TESTING)
    add_subdirectory (test)
endif ()
