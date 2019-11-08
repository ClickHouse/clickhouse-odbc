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
#     ODBC_MDAC_CLIENT_DEFINES
#     ODBC_MDAC_CLIENT_INCLUDE_DIRS
#     ODBC_MDAC_CLIENT_COMPILER_FLAGS
#     ODBC_MDAC_CLIENT_LINKER_FLAGS
#     ODBC_MDAC_CLIENT_LIBRARIES
#
#     ODBC_MDAC_DRIVER_DEFINES
#     ODBC_MDAC_DRIVER_INCLUDE_DIRS
#     ODBC_MDAC_DRIVER_COMPILER_FLAGS
#     ODBC_MDAC_DRIVER_LINKER_FLAGS
#     ODBC_MDAC_DRIVER_LIBRARIES
#

set(ODBC_MDAC_CLIENT_DEFINES)
set(ODBC_MDAC_CLIENT_INCLUDE_DIRS)
set(ODBC_MDAC_CLIENT_COMPILER_FLAGS)
set(ODBC_MDAC_CLIENT_LINKER_FLAGS)
set(ODBC_MDAC_CLIENT_LIBRARIES)

set(ODBC_MDAC_DRIVER_DEFINES)
set(ODBC_MDAC_DRIVER_INCLUDE_DIRS)
set(ODBC_MDAC_DRIVER_COMPILER_FLAGS)
set(ODBC_MDAC_DRIVER_LINKER_FLAGS)
set(ODBC_MDAC_DRIVER_LIBRARIES)

set (_client_headers sql.h;)
set (_client_libs odbc32;odbccp32)

if (MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    list (APPEND _client_libs ws2_32)
endif ()

# Starting from Visual Studio 2015, odbccp32 depends on symbols
# which are moved to legacy_stdio_definitions lib.
if (MSVC_TOOLSET_VERSION GREATER_EQUAL 140)
    list (APPEND _client_libs legacy_stdio_definitions)
endif ()

set (_driver_headers ${_client_headers})
set (_driver_libs ${_client_libs})


# ----------
foreach (_file ${_client_headers})
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
        list (APPEND ODBC_MDAC_CLIENT_INCLUDE_DIRS "${_path}")
    endif ()
endforeach ()

list (REMOVE_DUPLICATES ODBC_MDAC_CLIENT_INCLUDE_DIRS)
# ----------


# ----------
foreach (_file ${_client_libs})
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
        list (APPEND ODBC_MDAC_CLIENT_LIBRARIES "${_path}")
    endif ()
endforeach ()

list (REMOVE_DUPLICATES ODBC_MDAC_CLIENT_LIBRARIES)
# ----------


# ----------
foreach (_file ${_driver_headers})
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
        list (APPEND ODBC_MDAC_DRIVER_INCLUDE_DIRS "${_path}")
    endif ()
endforeach ()

list (REMOVE_DUPLICATES ODBC_MDAC_DRIVER_INCLUDE_DIRS)
# ----------

# ----------
foreach (_file ${_driver_libs})
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
        list (APPEND ODBC_MDAC_DRIVER_LIBRARIES "${_path}")
    endif ()
endforeach ()

list (REMOVE_DUPLICATES ODBC_MDAC_DRIVER_LIBRARIES)
# ----------


unset (_path CACHE)
unset (_file)
unset (_path)
unset (_client_headers)
unset (_client_libs)
unset (_driver_headers)
unset (_driver_libs)

if (
    ODBC_MDAC_CLIENT_INCLUDE_DIRS AND ODBC_MDAC_CLIENT_LIBRARIES AND
    ODBC_MDAC_DRIVER_INCLUDE_DIRS AND ODBC_MDAC_DRIVER_LIBRARIES
)
    set (ODBC_MDAC_FOUND TRUE)
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_MDAC REQUIRED_VARS ODBC_MDAC_FOUND)

mark_as_advanced (ODBC_MDAC_CLIENT_DEFINES)
mark_as_advanced (ODBC_MDAC_CLIENT_INCLUDE_DIRS)
mark_as_advanced (ODBC_MDAC_CLIENT_COMPILER_FLAGS)
mark_as_advanced (ODBC_MDAC_CLIENT_LINKER_FLAGS)
mark_as_advanced (ODBC_MDAC_CLIENT_LIBRARIES)

mark_as_advanced (ODBC_MDAC_DRIVER_DEFINES)
mark_as_advanced (ODBC_MDAC_DRIVER_INCLUDE_DIRS)
mark_as_advanced (ODBC_MDAC_DRIVER_COMPILER_FLAGS)
mark_as_advanced (ODBC_MDAC_DRIVER_LINKER_FLAGS)
mark_as_advanced (ODBC_MDAC_DRIVER_LIBRARIES)
