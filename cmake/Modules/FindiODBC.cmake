#
# Consults the following vars (if set):
#     ODBC_IODBC_DIR
#     ODBC_IODBC_CONFIG_SCRIPT
#
#     ODBC_IODBC_SKIP_CONFIG_SCRIPT
#     ODBC_IODBC_SKIP_PKG_CONFIG
#     ODBC_IODBC_SKIP_BREW
#
# Defines (some of) the following vars:
#     iODBC_FOUND
#     ODBC_IODBC_FOUND
#
#     ODBC_IODBC_DIR
#     ODBC_IODBC_CONFIG_SCRIPT
#     ODBC_IODBC_IODBCTEST
#     ODBC_IODBC_IODBCTESTW
#
#     ODBC_IODBC_APP_DEFINES
#     ODBC_IODBC_APP_INCLUDE_DIRS
#     ODBC_IODBC_APP_COMPILER_FLAGS
#     ODBC_IODBC_APP_LINKER_FLAGS
#
#     ODBC_IODBC_DRIVER_DEFINES
#     ODBC_IODBC_DRIVER_INCLUDE_DIRS
#     ODBC_IODBC_DRIVER_COMPILER_FLAGS
#     ODBC_IODBC_DRIVER_LINKER_FLAGS
#

include (cmake/extract_flags.cmake)

set (ODBC_IODBC_FOUND FALSE)

if (ODBC_IODBC_DIR AND ODBC_IODBC_CONFIG_SCRIPT)
    message (FATAL_ERROR "ODBC iODBC: Only one of ODBC_IODBC_DIR and ODBC_IODBC_CONFIG_SCRIPT can be specified at the same time.")
endif ()

if (ODBC_IODBC_SKIP_CONFIG_SCRIPT AND ODBC_IODBC_CONFIG_SCRIPT)
    message (FATAL_ERROR "ODBC iODBC: Only one of ODBC_IODBC_SKIP_CONFIG_SCRIPT and ODBC_IODBC_CONFIG_SCRIPT can be specified at the same time.")
endif ()

set (_path_hints)
if (APPLE AND NOT ODBC_IODBC_SKIP_BREW)
    find_program (BREW brew)
    mark_as_advanced (BREW)

    if (BREW)
        execute_process (
            COMMAND ${BREW} --prefix
            OUTPUT_VARIABLE _path_hints
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()
endif ()

set (_odbc_prefix)
if (ODBC_IODBC_DIR)
    set (_odbc_prefix "${ODBC_IODBC_DIR}")
endif ()

if (NOT ODBC_IODBC_CONFIG_SCRIPT AND NOT ODBC_IODBC_SKIP_PKG_CONFIG)
    unset (_libiodbc_pkg_config_dir CACHE)
    unset (_libiodbc_pkg_config_dir)

    find_program (PKG_CONFIG pkg-config)
    mark_as_advanced (PKG_CONFIG)

    if (ODBC_IODBC_DIR)
        find_path (_libiodbc_pkg_config_dir
            NAMES "libiodbc.pc"
            PATHS "${ODBC_IODBC_DIR}"
            PATH_SUFFIXES "lib/pkgconfig"
            NO_DEFAULT_PATH
        )
    else ()
        find_path (_libiodbc_pkg_config_dir
            NAMES "libiodbc.pc"
            HINTS ${_path_hints}
            PATH_SUFFIXES "lib/pkgconfig"
        )
    endif ()

    if (_pkg_config AND _libiodbc_pkg_config_dir)
        foreach (_role_lc app driver)
            foreach (_comp_lc defines include_dirs compiler_flags linker_flags)
                unset (_found_${_role_lc}_${_comp_lc})
            endforeach ()
        endforeach ()

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_libiodbc_pkg_config_dir} ${PKG_CONFIG} libiodbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_libiodbc_pkg_config_dir} ${PKG_CONFIG} libiodbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_cflags} ${_libs}"
            _found_app_defines
            _found_app_include_dirs
            _found_app_compiler_flags
            _found_app_linker_flags
        )

        set (_found_driver_defines "${_found_app_defines}")
        set (_found_driver_include_dirs "${_found_app_include_dirs}")
        set (_found_driver_compiler_flags "${_found_app_compiler_flags}")
        set (_found_driver_linker_flags "${_found_app_linker_flags}")

        list (FILTER _found_app_linker_flags EXCLUDE REGEX "^-liodbcinst$")
        list (FILTER _found_driver_linker_flags EXCLUDE REGEX "^-liodbc$")

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbc_pkg_config_dir} ${PKG_CONFIG} libiodbc --variable=prefix
            OUTPUT_VARIABLE _odbc_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        unset (_cflags)
        unset (_libs)

        set (ODBC_IODBC_FOUND TRUE)
    endif ()

    unset (_libiodbc_pkg_config_dir CACHE)
    unset (_libiodbc_pkg_config_dir)
endif ()

unset (_odbc_config_script CACHE)
unset (_odbc_config_script)

if (ODBC_IODBC_CONFIG_SCRIPT)
    set (_odbc_config_script "${ODBC_IODBC_CONFIG_SCRIPT}")
else ()
    if (_odbc_prefix)
        find_program (_odbc_config_script
            NAMES "iodbc-config"
            PATHS "${_odbc_prefix}"
            PATH_SUFFIXES "bin"
            NO_DEFAULT_PATH
        )
    else ()
        find_program (_odbc_config_script
            NAMES "iodbc-config"
            HINTS ${_path_hints}
        )
    endif ()
endif ()

if (NOT ODBC_IODBC_FOUND AND NOT ODBC_IODBC_SKIP_CONFIG_SCRIPT AND _odbc_config_script)
    foreach (_role_lc app driver)
        foreach (_comp_lc defines include_dirs compiler_flags linker_flags)
            unset (_found_${_role_lc}_${_comp_lc})
        endforeach ()
    endforeach ()

    execute_process (
        COMMAND ${_odbc_config_script} --libs
        OUTPUT_VARIABLE _libs
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process (
        COMMAND ${_odbc_config_script} --cflags
        OUTPUT_VARIABLE _cflags
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    extract_flags ("${_cflags} ${_libs}"
        _found_app_defines
        _found_app_include_dirs
        _found_app_compiler_flags
        _found_app_linker_flags
    )

    set (_found_driver_defines "${_found_app_defines}")
    set (_found_driver_include_dirs "${_found_app_include_dirs}")
    set (_found_driver_compiler_flags "${_found_app_compiler_flags}")
    set (_found_driver_linker_flags "${_found_app_linker_flags}")

    list (FILTER _found_app_linker_flags EXCLUDE REGEX "^-liodbcinst$")
    list (FILTER _found_driver_linker_flags EXCLUDE REGEX "^-liodbc$")

    execute_process (
        COMMAND ${_odbc_config_script} --prefix
        OUTPUT_VARIABLE _odbc_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    unset (_cflags)
    unset (_libs)

    set (ODBC_IODBC_FOUND TRUE)
endif ()

if (NOT ODBC_IODBC_FOUND AND NOT ODBC_IODBC_CONFIG_SCRIPT)
    set (ODBC_IODBC_FOUND TRUE)

    foreach (_role_lc app driver)
        set (_headers sql.h;sqltypes.h;sqlucode.h;sqlext.h)
        set (_libs)

        if ("${_role_lc}" STREQUAL "app")
            list (APPEND _libs iodbc)
        elseif ("${_role_lc}" STREQUAL "driver")
            list (APPEND _headers odbcinst.h)
            list (APPEND _libs iodbcinst)
        endif ()

        set(_found_${_role_lc}_include_dirs)
        foreach (_file ${_headers})
            unset (_path CACHE)
            unset (_path)

            if (ODBC_IODBC_DIR)
                find_path (_path
                    NAMES "${_file}"
                    PATHS "${ODBC_IODBC_DIR}"
                    NO_DEFAULT_PATH
                )
            else ()
                find_path (_path
                    NAMES "${_file}"
                    HINTS ${_path_hints}
                )
            endif ()

            if (_path)
                list (APPEND _found_${_role_lc}_include_dirs "${_path}")
            else ()
                set (ODBC_IODBC_FOUND FALSE)
                message(WARNING "ODBC iODBC: Failed to locate path containing '${_file}' header file.")
            endif ()

            if ("${_file}" STREQUAL "sql.h")
                if (NOT EXISTS "${_path}/iodbcext.h")
                    set (ODBC_IODBC_FOUND FALSE)
                    message(WARNING "ODBC iODBC: path containing '${_file}' header file doesn't look like an include path for iODBC headers. (Possible clash with UnixODBC installation?)")
                else ()
                    get_filename_component(_odbc_prefix "${_path}/sql.h" REALPATH)
                    get_filename_component(_odbc_prefix "${_odbc_prefix}" DIRECTORY)
                    get_filename_component(_odbc_prefix "${_odbc_prefix}" DIRECTORY)
                endif ()
            endif ()
        endforeach ()
        list (REMOVE_DUPLICATES _found_${_role_lc}_include_dirs)

        set(_found_${_role_lc}_linker_flags)
        foreach (_file ${_libs})
            unset (_path CACHE)
            unset (_path)

            if (ODBC_IODBC_DIR)
                find_library (_path
                    NAMES "${_file}"
                    PATHS "${ODBC_IODBC_DIR}"
                    NO_DEFAULT_PATH
                )
            else ()
                find_library (_path
                    NAMES "${_file}"
                    HINTS ${_path_hints}
                )
            endif ()

            if (_path)
                list (APPEND _found_${_role_lc}_linker_flags "${_path}")
            else ()
                set (ODBC_IODBC_FOUND FALSE)
                message(WARNING "ODBC iODBC: Failed to locate '${_file}' library file.")
            endif ()
        endforeach ()
        list (REMOVE_DUPLICATES _found_${_role_lc}_linker_flags)
    endforeach ()

    unset (_path CACHE)
    unset (_path)
    unset (_file)
    unset (_libs)
    unset (_headers)
endif ()

if (ODBC_IODBC_FOUND AND _odbc_prefix)
    set (ODBC_IODBC_DIR "${_odbc_prefix}")

    unset (_iodbctest CACHE)
    unset (_iodbctest)

    find_program (_iodbctest
        NAMES "iodbctest"
        PATHS "${ODBC_IODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    if (_iodbctest)
        set (ODBC_IODBC_IODBCTEST "${_iodbctest}")
    endif ()

    unset (_iodbctest CACHE)
    unset (_iodbctest)

    find_program (_iodbctestw
        NAMES "iodbctestw"
        PATHS "${ODBC_IODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    if (_iodbctestw)
        set (ODBC_IODBC_IODBCTESTW "${_iodbctestw}")
    endif ()

    unset (_iodbctestw CACHE)
    unset (_iodbctestw)
endif ()

if (ODBC_IODBC_FOUND AND _odbc_config_script)
    set (ODBC_IODBC_CONFIG_SCRIPT "${_odbc_config_script}")
endif ()

unset (_odbc_config_script CACHE)
unset (_odbc_config_script)
unset (_odbc_prefix)
unset (_path_hints)

if (ODBC_IODBC_FOUND)
    foreach (_role_uc APP DRIVER)
        string (TOLOWER "${_role_uc}" _role_lc)
        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LINKER_FLAGS)
            string (TOLOWER "${_comp_uc}" _comp_lc)
            set (ODBC_IODBC_${_role_uc}_${_comp_uc})
            if (_found_${_role_lc}_${_comp_lc})
                set (ODBC_IODBC_${_role_uc}_${_comp_uc} ${_found_${_role_lc}_${_comp_lc}})
            endif ()
            mark_as_advanced (ODBC_IODBC_${_role_uc}_${_comp_uc})
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
find_package_handle_standard_args (iODBC REQUIRED_VARS ODBC_IODBC_FOUND)

set (ODBC_IODBC_FOUND "${iODBC_FOUND}")
