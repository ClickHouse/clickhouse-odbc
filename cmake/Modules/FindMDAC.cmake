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
#     ODBC_MDAC_APP_LIBRARIES
#
#     ODBC_MDAC_DRIVER_DEFINES
#     ODBC_MDAC_DRIVER_INCLUDE_DIRS
#     ODBC_MDAC_DRIVER_COMPILER_FLAGS
#     ODBC_MDAC_DRIVER_LINKER_FLAGS
#     ODBC_MDAC_DRIVER_LIBRARIES
#

set (ODBC_MDAC_FOUND TRUE)

foreach (_role_uc APP DRIVER)
    if (NOT WIN32)
        set (ODBC_MDAC_FOUND FALSE)
        message(WARNING "ODBC MDAC: Not available on non-Windows systems.")
        break ()
    endif ()

    set (_headers sql.h;sqltypes.h;sqlucode.h;sqlext.h)
    set (_libs)

    if ("${_role_uc}" STREQUAL "APP")
        list (APPEND _libs odbc32)
    elseif ("${_role_uc}" STREQUAL "DRIVER")
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

    set(_found_include_dirs_${_role_uc})
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
            list (APPEND _found_include_dirs_${_role_uc} "${_path}")
        else ()
            set (ODBC_MDAC_FOUND FALSE)
            message(WARNING "ODBC MDAC: Failed to locate path containing '${_file}' header file.")
        endif ()
    endforeach ()

    set(_found_libs_${_role_uc})
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
            list (APPEND _found_libs_${_role_uc} "${_path}")
        else ()
            set (ODBC_MDAC_FOUND FALSE)
            message(WARNING "ODBC MDAC: Failed to locate '${_file}' library file.")
        endif ()
    endforeach ()
endforeach ()

if (ODBC_MDAC_FOUND)
    foreach (_role_uc APP DRIVER)
        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LIBRARIES LINKER_FLAGS)
            unset (ODBC_MDAC_${_role_uc}_${_comp_uc})
        endforeach ()

        set (ODBC_MDAC_${_role_uc}_INCLUDE_DIRS ${_found_include_dirs_${_role_uc}})
        set (ODBC_MDAC_${_role_uc}_LIBRARIES ${_found_libs_${_role_uc}})

        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LIBRARIES LINKER_FLAGS)
            if (ODBC_MDAC_${_role_uc}_${_comp_uc})
                list (REMOVE_DUPLICATES ODBC_MDAC_${_role_uc}_${_comp_uc})
                mark_as_advanced (ODBC_MDAC_${_role_uc}_${_comp_uc})
            endif ()
        endforeach ()
    endforeach ()
endif ()

unset (_headers)
unset (_libs)
unset (_file)
unset (_path CACHE)
unset (_path)
foreach (_role_uc APP DRIVER)
    unset (_found_include_dirs_${_role_uc})
    unset (_found_libs_${_role_uc})
endforeach ()
unset (_role_uc)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_MDAC REQUIRED_VARS ODBC_MDAC_FOUND)
