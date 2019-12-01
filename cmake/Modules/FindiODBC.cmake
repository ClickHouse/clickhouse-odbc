if (ODBC_IODBC_FOUND)
    return ()
endif ()

#
# Consults (if present) the following vars:
#     ODBC_IODBC_DIR
#
#     ODBC_IODBC_CONFIG
#
# Defines (some of) the following vars:
#     ODBC_IODBC_FOUND
#
#     ODBC_IODBC_DEFINES
#     ODBC_IODBC_INCLUDE_DIRS
#     ODBC_IODBC_COMPILER_FLAGS
#     ODBC_IODBC_LINKER_FLAGS
#     ODBC_IODBC_LIBRARIES
#

if (ODBC_IODBC_DIR AND ODBC_IODBC_CONFIG)
    message (FATAL_ERROR "Only one of ODBC_IODBC_DIR and ODBC_IODBC_CONFIG can be specified at the same time.")
endif ()

set(ODBC_IODBC_DEFINES)
set(ODBC_IODBC_INCLUDE_DIRS)
set(ODBC_IODBC_COMPILER_FLAGS)
set(ODBC_IODBC_LINKER_FLAGS)
set(ODBC_IODBC_LIBRARIES)

unset (_config CACHE)
unset (_config)

unset (_pc_dir CACHE)
unset (_pc_dir)

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_cflags CACHE)
unset (_cflags)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

find_program(PKG_CONFIG pkg-config)
mark_as_advanced (PKG_CONFIG)

include (cmake/extract_flags.cmake)

if (ODBC_IODBC_CONFIG)
    set (_config "${ODBC_IODBC_CONFIG}")
elseif (ODBC_IODBC_DIR)
    find_program (_config
        NAMES "iodbc-config"
        PATHS "${ODBC_IODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    find_path (_pc_dir
        NAMES "libiodbc.pc"
        PATHS "${ODBC_IODBC_DIR}"
        PATH_SUFFIXES "lib/pkgconfig"
        NO_DEFAULT_PATH
    )
else ()
    find_program (_config
        NAMES "iodbc-config"
    )
endif ()

if (_config)
    execute_process (
        COMMAND ${_config} --libs
        OUTPUT_VARIABLE _libs
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process (
        COMMAND ${_config} --cflags
        OUTPUT_VARIABLE _cflags
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    extract_flags ("${_libs} ${_cflags}"
        ODBC_IODBC_DEFINES
        ODBC_IODBC_INCLUDE_DIRS
        ODBC_IODBC_COMPILER_FLAGS
        ODBC_IODBC_LINKER_FLAGS
        ODBC_IODBC_LIBRARIES
    )
elseif (PKG_CONFIG)
    if (_pc_dir)
        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir} ${PKG_CONFIG} libiodbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir} ${PKG_CONFIG} libiodbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_IODBC_DEFINES
            ODBC_IODBC_INCLUDE_DIRS
            ODBC_IODBC_COMPILER_FLAGS
            ODBC_IODBC_LINKER_FLAGS
            ODBC_IODBC_LIBRARIES
        )
    elseif (NOT ODBC_IODBC_DIR)
        execute_process (
            COMMAND ${PKG_CONFIG} libiodbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND ${PKG_CONFIG} libiodbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_IODBC_DEFINES
            ODBC_IODBC_INCLUDE_DIRS
            ODBC_IODBC_COMPILER_FLAGS
            ODBC_IODBC_LINKER_FLAGS
            ODBC_IODBC_LIBRARIES
        )
    endif ()
endif ()

if (NOT ODBC_IODBC_INCLUDE_DIRS)
    set (_headers sql.h;)

    foreach (_file ${_headers})
        unset (_path CACHE)
        unset (_path)

        if (ODBC_IODBC_DIR)
            find_path (_path
                NAMES "${_file}"
                PATHS "${ODBC_IODBC_DIR}"
                PATH_SUFFIXES "include"
                NO_DEFAULT_PATH
            )
        else ()
            find_path (_path
                NAMES "${_file}"
            )
        endif ()

        if (_path)
            list (APPEND ODBC_IODBC_INCLUDE_DIRS "${_path}")
        endif ()
    endforeach ()
endif ()

if (NOT ODBC_IODBC_LIBRARIES)
    set (_libs iodbc;iodbcinst)

    foreach (_file ${_libs})
        unset (_path CACHE)
        unset (_path)

        if (ODBC_IODBC_DIR)
            find_library (_path
                NAMES "${_file}"
                PATHS "${ODBC_IODBC_DIR}"
                PATH_SUFFIXES "lib"
                NO_DEFAULT_PATH
            )
        else ()
            find_library (_path
                NAMES "${_file}"
            )
        endif ()

        if (_path)
            list (APPEND ODBC_IODBC_LIBRARIES "${_path}")
        endif ()
    endforeach ()
endif ()

if (ODBC_IODBC_INCLUDE_DIRS AND ODBC_IODBC_LIBRARIES)
    set (ODBC_IODBC_FOUND TRUE)
endif ()

unset (_config CACHE)
unset (_config)

unset (_pc_dir CACHE)
unset (_pc_dir)

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_cflags CACHE)
unset (_cflags)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

if (UNIX)
    list (APPEND ODBC_IODBC_LIBRARIES dl)
endif ()

if (ODBC_IODBC_DEFINES)
    list (REMOVE_DUPLICATES ODBC_IODBC_DEFINES)
endif()

if (ODBC_IODBC_INCLUDE_DIRS)
    list (REMOVE_DUPLICATES ODBC_IODBC_INCLUDE_DIRS)
endif()

if (ODBC_IODBC_COMPILER_FLAGS)
    list (REMOVE_DUPLICATES ODBC_IODBC_COMPILER_FLAGS)
endif()

if (ODBC_IODBC_LINKER_FLAGS)
    list (REMOVE_DUPLICATES ODBC_IODBC_LINKER_FLAGS)
endif()

if (ODBC_IODBC_LIBRARIES)
    list (REMOVE_DUPLICATES ODBC_IODBC_LIBRARIES)
endif()

mark_as_advanced (ODBC_IODBC_DEFINES)
mark_as_advanced (ODBC_IODBC_INCLUDE_DIRS)
mark_as_advanced (ODBC_IODBC_COMPILER_FLAGS)
mark_as_advanced (ODBC_IODBC_LINKER_FLAGS)
mark_as_advanced (ODBC_IODBC_LIBRARIES)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_IODBC REQUIRED_VARS ODBC_IODBC_FOUND)
