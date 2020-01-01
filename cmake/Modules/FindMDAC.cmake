#
# Consults (some of) the following vars (if set):
#     ODBC_MDAC_DIR
#
# Defines (some of) the following vars:
#     ODBC_MDAC_FOUND
#
#     ODBC_MDAC_APP_DEFINES
#     ODBC_MDAC_APP_INCLUDE_DIRS
#     ODBC_MDAC_APP_COMPILER_FLAGS
#     ODBC_MDAC_APP_LINKER_FLAGS
#
#     ODBC_MDAC_DRIVER_DEFINES
#     ODBC_MDAC_DRIVER_INCLUDE_DIRS
#     ODBC_MDAC_DRIVER_COMPILER_FLAGS
#     ODBC_MDAC_DRIVER_LINKER_FLAGS
#

set (ODBC_MDAC_FOUND TRUE)

foreach (_role_lc app driver)
    if (NOT WIN32)
        set (ODBC_MDAC_FOUND FALSE)
        message(WARNING "ODBC MDAC: Not available on non-Windows systems.")
        break ()
    endif ()

    set (_headers sql.h;sqltypes.h;sqlucode.h;sqlext.h)
    set (_libs)

    if ("${_role_lc}" STREQUAL "app")
        list (APPEND _libs odbc32)
    elseif ("${_role_lc}" STREQUAL "driver")
        list (APPEND _headers odbcinst.h)
        list (APPEND _libs odbccp32)

        if (MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
            list (APPEND _libs ws2_32)
        endif ()

        # Starting from Visual Studio 2015, odbccp32 depends on symbols
        # which are moved to legacy_stdio_definitions lib.
        if (MSVC_TOOLSET_VERSION GREATER_EQUAL 140)
            list (APPEND _libs legacy_stdio_definitions)
        endif ()
    endif ()

    set(_found_${_role_lc}_include_dirs)
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
            list (APPEND _found_${_role_lc}_include_dirs "${_path}")
        else ()
            set (ODBC_MDAC_FOUND FALSE)
            message(WARNING "ODBC MDAC: Failed to locate path containing '${_file}' header file.")
        endif ()
    endforeach ()
    list (REMOVE_DUPLICATES _found_${_role_lc}_include_dirs)

    set(_found_${_role_lc}_linker_flags)
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
            list (APPEND _found_${_role_lc}_linker_flags "${_path}")
        else ()
            set (ODBC_MDAC_FOUND FALSE)
            message(WARNING "ODBC MDAC: Failed to locate '${_file}' library file.")
        endif ()
    endforeach ()
    list (REMOVE_DUPLICATES _found_${_role_lc}_linker_flags)
endforeach ()

unset (_path CACHE)
unset (_path)
unset (_file)
unset (_libs)
unset (_headers)

if (ODBC_MDAC_FOUND)
    foreach (_role_uc APP DRIVER)
        string (TOLOWER "${_role_uc}" _role_lc)
        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LINKER_FLAGS)
            string (TOLOWER "${_comp_uc}" _comp_lc)
            set (ODBC_MDAC_${_role_uc}_${_comp_uc})
            if (_found_${_role_lc}_${_comp_lc})
                set (ODBC_MDAC_${_role_uc}_${_comp_uc} ${_found_${_role_lc}_${_comp_lc}})
            endif ()
            mark_as_advanced (ODBC_MDAC_${_role_uc}_${_comp_uc})
        endforeach ()
    endforeach ()
endif ()

foreach (_role_lc app driver)
    foreach (_comp_lc defines include_dirs compiler_flags linker_flags)
        unset (_found_${_role_lc}_${_comp_lc})
    endforeach ()
endforeach ()

unset (_comp_lc)
unset (_comp_uc)
unset (_role_lc)
unset (_role_uc)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_MDAC REQUIRED_VARS ODBC_MDAC_FOUND)
