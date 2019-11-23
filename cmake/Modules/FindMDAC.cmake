if (ODBC_MDAC_FOUND)
    return ()
endif ()

#
# Consults (if present) the following vars:
#     ODBC_MDAC_DIR
#
# Defines (some of) the following vars:
#     ODBC_MDAC_FOUND
#
#     ODBC_MDAC_DEFINES
#     ODBC_MDAC_INCLUDE_DIRS
#     ODBC_MDAC_COMPILER_FLAGS
#     ODBC_MDAC_LINKER_FLAGS
#     ODBC_MDAC_LIBRARIES
#

set(ODBC_MDAC_DEFINES)
set(ODBC_MDAC_INCLUDE_DIRS)
set(ODBC_MDAC_COMPILER_FLAGS)
set(ODBC_MDAC_LINKER_FLAGS)
set(ODBC_MDAC_LIBRARIES)

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

set (_headers sql.h;)
set (_libs odbc32;odbccp32)

if (MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    list (APPEND _libs ws2_32)
endif ()

# Starting from Visual Studio 2015, odbccp32 depends on symbols
# which are moved to legacy_stdio_definitions lib.
if (MSVC_TOOLSET_VERSION GREATER_EQUAL 140)
    list (APPEND _libs legacy_stdio_definitions)
endif ()

foreach (_file ${_headers})
    unset (_path CACHE)
    unset (_path)

    if (ODBC_MDAC_DIR)
        find_path (_path
            NAMES "${_file}"
            PATHS "${ODBC_MDAC_DIR}"
            NO_DEFAULT_PATH
        )
    else ()
        find_path (_path
            NAMES "${_file}"
        )
    endif ()

    if (_path)
        list (APPEND ODBC_MDAC_INCLUDE_DIRS "${_path}")
    endif ()
endforeach ()

foreach (_file ${_libs})
    unset (_path CACHE)
    unset (_path)

    if (ODBC_MDAC_DIR)
        find_library (_path
            NAMES "${_file}"
            PATHS "${ODBC_MDAC_DIR}"
            NO_DEFAULT_PATH
        )
    else ()
        find_library (_path
            NAMES "${_file}"
        )
    endif ()

    if (_path)
        list (APPEND ODBC_MDAC_LIBRARIES "${_path}")
    endif ()
endforeach ()

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

list (REMOVE_DUPLICATES ODBC_MDAC_DEFINES)
list (REMOVE_DUPLICATES ODBC_MDAC_INCLUDE_DIRS)
list (REMOVE_DUPLICATES ODBC_MDAC_COMPILER_FLAGS)
list (REMOVE_DUPLICATES ODBC_MDAC_LINKER_FLAGS)
list (REMOVE_DUPLICATES ODBC_MDAC_LIBRARIES)

mark_as_advanced (ODBC_MDAC_DEFINES)
mark_as_advanced (ODBC_MDAC_INCLUDE_DIRS)
mark_as_advanced (ODBC_MDAC_COMPILER_FLAGS)
mark_as_advanced (ODBC_MDAC_LINKER_FLAGS)
mark_as_advanced (ODBC_MDAC_LIBRARIES)

if (ODBC_MDAC_INCLUDE_DIRS AND ODBC_MDAC_LIBRARIES)
    set (ODBC_MDAC_FOUND TRUE)
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_MDAC REQUIRED_VARS ODBC_MDAC_FOUND)
