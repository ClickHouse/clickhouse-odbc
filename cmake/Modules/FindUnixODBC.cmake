#
# Consults the following vars (if set):
#     ODBC_UNIXODBC_DIR
#     ODBC_UNIXODBC_CONFIG_SCRIPT
#
#     ODBC_UNIXODBC_SKIP_CONFIG_SCRIPT
#     ODBC_UNIXODBC_SKIP_PKG_CONFIG
#     ODBC_UNIXODBC_SKIP_BREW
#
# Defines (some of) the following vars:
#     ODBC_UNIXODBC_FOUND
#
#     ODBC_UNIXODBC_DIR
#     ODBC_UNIXODBC_CONFIG_SCRIPT
#     ODBC_UNIXODBC_ISQL
#     ODBC_UNIXODBC_IUSQL
#
#     ODBC_UNIXODBC_APP_DEFINES
#     ODBC_UNIXODBC_APP_INCLUDE_DIRS
#     ODBC_UNIXODBC_APP_COMPILER_FLAGS
#     ODBC_UNIXODBC_APP_LINKER_FLAGS
#
#     ODBC_UNIXODBC_DRIVER_DEFINES
#     ODBC_UNIXODBC_DRIVER_INCLUDE_DIRS
#     ODBC_UNIXODBC_DRIVER_COMPILER_FLAGS
#     ODBC_UNIXODBC_DRIVER_LINKER_FLAGS
#

include (cmake/extract_flags.cmake)

set (ODBC_UNIXODBC_FOUND FALSE)

if (ODBC_UNIXODBC_DIR AND ODBC_UNIXODBC_CONFIG_SCRIPT)
    message (FATAL_ERROR "ODBC UnixODBC: Only one of ODBC_UNIXODBC_DIR and ODBC_UNIXODBC_CONFIG_SCRIPT can be specified at the same time.")
endif ()

if (ODBC_UNIXODBC_SKIP_CONFIG_SCRIPT AND ODBC_UNIXODBC_CONFIG_SCRIPT)
    message (FATAL_ERROR "ODBC UnixODBC: Only one of ODBC_UNIXODBC_SKIP_CONFIG_SCRIPT and ODBC_UNIXODBC_CONFIG_SCRIPT can be specified at the same time.")
endif ()

set (_path_hints)
if (APPLE AND NOT ODBC_UNIXODBC_SKIP_BREW)
    find_program (_brew brew)
    mark_as_advanced (_brew)

    if (_brew)
        execute_process (
            COMMAND ${_brew} --prefix
            OUTPUT_VARIABLE _path_hints
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()
endif ()

set (_odbc_prefix)
if (ODBC_UNIXODBC_DIR)
    set (_odbc_prefix "${ODBC_UNIXODBC_DIR}")
endif ()

if (NOT ODBC_UNIXODBC_CONFIG_SCRIPT AND NOT ODBC_UNIXODBC_SKIP_PKG_CONFIG)
    unset (_odbc_pkg_config_dir CACHE)
    unset (_odbc_pkg_config_dir)
    unset (_odbcinst_pkg_config_dir CACHE)
    unset (_odbcinst_pkg_config_dir)

    find_program (_pkg_config pkg-config)
    mark_as_advanced (_pkg_config)

    if (ODBC_UNIXODBC_DIR)
        find_path (_odbc_pkg_config_dir
            NAMES "odbc.pc"
            PATHS "${ODBC_UNIXODBC_DIR}"
            PATH_SUFFIXES "lib/pkgconfig"
            NO_DEFAULT_PATH
        )

        find_path (_odbcinst_pkg_config_dir
            NAMES "odbcinst.pc"
            PATHS "${ODBC_UNIXODBC_DIR}"
            PATH_SUFFIXES "lib/pkgconfig"
            NO_DEFAULT_PATH
        )
    else ()
        find_path (_odbc_pkg_config_dir
            NAMES "odbc.pc"
            HINTS ${_path_hints}
            PATH_SUFFIXES "lib/pkgconfig"
        )

        find_path (_odbcinst_pkg_config_dir
            NAMES "odbcinst.pc"
            HINTS ${_path_hints}
            PATH_SUFFIXES "lib/pkgconfig"
        )
    endif ()

    if (_pkg_config AND _odbc_pkg_config_dir AND _odbcinst_pkg_config_dir)
        foreach (_role_lc app driver)
            foreach (_comp_lc defines include_dirs compiler_flags linker_flags)
                unset (_found_${_role_lc}_${_comp_lc})
            endforeach ()
        endforeach ()

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbc_pkg_config_dir} ${_pkg_config} odbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbc_pkg_config_dir} ${_pkg_config} odbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbc_pkg_config_dir} ${_pkg_config} odbc --variable=ulen
            OUTPUT_VARIABLE _ulen
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_ulen} ${_cflags} ${_libs}"
            _found_app_defines
            _found_app_include_dirs
            _found_app_compiler_flags
            _found_app_linker_flags
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbcinst_pkg_config_dir} ${_pkg_config} odbcinst --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbcinst_pkg_config_dir} ${_pkg_config} odbcinst --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_cflags} ${_libs}"
            _found_driver_defines
            _found_driver_include_dirs
            _found_driver_compiler_flags
            _found_driver_linker_flags
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_odbc_pkg_config_dir} ${_pkg_config} odbc --variable=prefix
            OUTPUT_VARIABLE _odbc_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        unset (_ulen)
        unset (_cflags)
        unset (_libs)

        set (ODBC_UNIXODBC_FOUND TRUE)
    endif ()

    unset (_odbcinst_pkg_config_dir CACHE)
    unset (_odbcinst_pkg_config_dir)
    unset (_odbc_pkg_config_dir CACHE)
    unset (_odbc_pkg_config_dir)
endif ()

unset (_odbc_config_script CACHE)
unset (_odbc_config_script)

if (ODBC_UNIXODBC_CONFIG_SCRIPT)
    set (_odbc_config_script "${ODBC_UNIXODBC_CONFIG_SCRIPT}")
else ()
    if (_odbc_prefix)
        find_program (_odbc_config_script
            NAMES "odbc_config"
            PATHS "${_odbc_prefix}"
            PATH_SUFFIXES "bin"
            NO_DEFAULT_PATH
        )
    else ()
        find_program (_odbc_config_script
            NAMES "odbc_config"
            HINTS ${_path_hints}
        )
    endif ()
endif ()

if (NOT ODBC_UNIXODBC_FOUND AND NOT ODBC_UNIXODBC_SKIP_CONFIG_SCRIPT AND _odbc_config_script)
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

    execute_process (
        COMMAND ${_odbc_config_script} --ulen
        OUTPUT_VARIABLE _ulen
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    extract_flags ("${_ulen} ${_cflags} ${_libs}"
        _found_app_defines
        _found_app_include_dirs
        _found_app_compiler_flags
        _found_app_linker_flags
    )

    # set (_found_driver_defines "${_found_app_defines}")
    # set (_found_driver_include_dirs "${_found_app_include_dirs}")
    # set (_found_driver_compiler_flags "${_found_app_compiler_flags}")
    # set (_found_driver_linker_flags "{_found_app_linker_flags}")

    # list (FILTER _found_app_linker_flags EXCLUDE REGEX "^-lodbcinst$")
    # if ("${_found_app_linker_flags}" STREQUAL "${_found_driver_linker_flags}")
    #     list (APPEND _found_driver_linker_flags "-lodbcinst")
    # endif ()
    # list (FILTER _found_driver_linker_flags EXCLUDE REGEX "^-lodbc$")

    extract_flags ("${_libs}"
        _found_driver_defines
        _found_driver_include_dirs
        _found_driver_compiler_flags
        _found_driver_linker_flags
    )

    list (TRANSFORM _found_driver_linker_flags REPLACE "^-lodbc$" "-lodbcinst")

    execute_process (
        COMMAND ${_odbc_config_script} --prefix
        OUTPUT_VARIABLE _odbc_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    unset (_ulen)
    unset (_cflags)
    unset (_libs)

    set (ODBC_UNIXODBC_FOUND TRUE)
endif ()

if (NOT ODBC_UNIXODBC_FOUND AND NOT ODBC_UNIXODBC_CONFIG_SCRIPT)
    set (ODBC_UNIXODBC_FOUND TRUE)

    foreach (_role_lc app driver)
        set (_headers sql.h;sqltypes.h;sqlucode.h;sqlext.h)
        set (_libs)

        if ("${_role_lc}" STREQUAL "app")
            list (APPEND _libs odbc)
        elseif ("${_role_lc}" STREQUAL "driver")
            list (APPEND _headers odbcinst.h)
            list (APPEND _libs odbcinst)
        endif ()

        set(_found_${_role_lc}_include_dirs)
        foreach (_file ${_headers})
            unset (_path CACHE)
            unset (_path)

            if (ODBC_UNIXODBC_DIR)
                find_path (_path
                    NAMES "${_file}"
                    PATHS "${ODBC_UNIXODBC_DIR}"
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
                set (ODBC_UNIXODBC_FOUND FALSE)
                message(WARNING "ODBC UnixODBC: Failed to locate path containing '${_file}' header file.")
            endif ()

            if ("${_file}" STREQUAL "sql.h")
                if (NOT EXISTS "${_path}/uodbc_extras.h")
                    set (ODBC_UNIXODBC_FOUND FALSE)
                    message(WARNING "ODBC UnixODBC: path containing '${_file}' header file doesn't look like an include path for UnixODBC headers. (Possible clash with iODBC installation?)")
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

            if (ODBC_UNIXODBC_DIR)
                find_library (_path
                    NAMES "${_file}"
                    PATHS "${ODBC_UNIXODBC_DIR}"
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
                set (ODBC_UNIXODBC_FOUND FALSE)
                message(WARNING "ODBC UnixODBC: Failed to locate '${_file}' library file.")
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

if (ODBC_UNIXODBC_FOUND)




# ltdl






endif ()

if (ODBC_UNIXODBC_FOUND AND _odbc_prefix)
    set (ODBC_UNIXODBC_DIR "${_odbc_prefix}")

    unset (_isql CACHE)
    unset (_isql)

    find_program (_isql
        NAMES "isql"
        PATHS "${ODBC_UNIXODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    if (_isql)
        set (ODBC_UNIXODBC_ISQL "${_isql}")
    endif ()

    unset (_isql CACHE)
    unset (_isql)

    find_program (_iusql
        NAMES "iusql"
        PATHS "${ODBC_UNIXODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    if (_iusql)
        set (ODBC_UNIXODBC_IUSQL "${_iusql}")
    endif ()

    unset (_iusql CACHE)
    unset (_iusql)
endif ()

if (ODBC_UNIXODBC_FOUND AND _odbc_config_script)
    set (ODBC_UNIXODBC_CONFIG_SCRIPT "${_odbc_config_script}")
endif ()

unset (_odbc_config_script CACHE)
unset (_odbc_config_script)
unset (_odbc_prefix)
unset (_path_hints)

if (ODBC_UNIXODBC_FOUND)
    foreach (_role_uc APP DRIVER)
        string (TOLOWER "${_role_uc}" _role_lc)
        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LINKER_FLAGS)
            string (TOLOWER "${_comp_uc}" _comp_lc)
            set (ODBC_UNIXODBC_${_role_uc}_${_comp_uc})
            if (_found_${_role_lc}_${_comp_lc})
                set (ODBC_UNIXODBC_${_role_uc}_${_comp_uc} ${_found_${_role_lc}_${_comp_lc}})
            endif ()
            mark_as_advanced (ODBC_UNIXODBC_${_role_uc}_${_comp_uc})
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
find_package_handle_standard_args (ODBC_UNIXODBC REQUIRED_VARS ODBC_UNIXODBC_FOUND)
