if (ODBC_UNIXODBC_FOUND)
    return ()
endif ()

#
# Consults (if present) the following vars:
#     ODBC_UNIXODBC_DIR
#
#     ODBC_UNIXODBC_CONFIG
#
# Defines (some of) the following vars:
#     ODBC_UNIXODBC_FOUND
#
#     ODBC_UNIXODBC_DEFINES
#     ODBC_UNIXODBC_INCLUDE_DIRS
#     ODBC_UNIXODBC_COMPILER_FLAGS
#     ODBC_UNIXODBC_LINKER_FLAGS
#     ODBC_UNIXODBC_LIBRARIES
#

if (ODBC_UNIXODBC_DIR AND ODBC_UNIXODBC_CONFIG)
    message (FATAL_ERROR "Only one of ODBC_UNIXODBC_DIR and ODBC_UNIXODBC_CONFIG can be specified at the same time")
endif ()

set(ODBC_UNIXODBC_DEFINES)
set(ODBC_UNIXODBC_INCLUDE_DIRS)
set(ODBC_UNIXODBC_COMPILER_FLAGS)
set(ODBC_UNIXODBC_LINKER_FLAGS)
set(ODBC_UNIXODBC_LIBRARIES)

unset (_config CACHE)
unset (_config)

unset (_pc_dir CACHE)
unset (_pc_dir)

unset (_pc_dir_inst CACHE)
unset (_pc_dir_inst)

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_cflags CACHE)
unset (_cflags)

unset (_ulen CACHE)
unset (_ulen)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

find_program(PKG_CONFIG pkg-config)
mark_as_advanced (PKG_CONFIG)

include (cmake/extract_flags.cmake)

if (ODBC_UNIXODBC_CONFIG)
    set (_config "${ODBC_UNIXODBC_CONFIG}")
elseif (ODBC_UNIXODBC_DIR)
    find_program (_config
        NAMES "odbc_config"
        PATHS "${ODBC_UNIXODBC_DIR}"
        PATH_SUFFIXES "bin"
        NO_DEFAULT_PATH
    )

    find_path (_pc_dir
        NAMES "odbc.pc"
        PATHS "${ODBC_UNIXODBC_DIR}"
        PATH_SUFFIXES "lib/pkgconfig"
        NO_DEFAULT_PATH
    )

    find_path (_pc_dir_inst
        NAMES "odbcinst.pc"
        PATHS "${ODBC_UNIXODBC_DIR}"
        PATH_SUFFIXES "lib/pkgconfig"
        NO_DEFAULT_PATH
    )

    if (NOT _pc_dir OR NOT _pc_dir_inst)
        unset (_pc_dir CACHE)
        unset (_pc_dir)
        
        unset (_pc_dir_inst CACHE)
        unset (_pc_dir_inst)
    endif ()
else ()
    find_program (_config
        NAMES "odbc_config"
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

    execute_process (
        COMMAND ${_config} --ulen
        OUTPUT_VARIABLE _ulen
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    extract_flags ("${_libs} ${_cflags} ${_ulen} -lodbcinst"
        ODBC_UNIXODBC_DEFINES
        ODBC_UNIXODBC_INCLUDE_DIRS
        ODBC_UNIXODBC_COMPILER_FLAGS
        ODBC_UNIXODBC_LINKER_FLAGS
        ODBC_UNIXODBC_LIBRARIES
    )
elseif (PKG_CONFIG)
    if (_pc_dir OR _pc_dir_inst)
        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir} ${PKG_CONFIG} odbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir} ${PKG_CONFIG} odbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_UNIXODBC_DEFINES
            ODBC_UNIXODBC_INCLUDE_DIRS
            ODBC_UNIXODBC_COMPILER_FLAGS
            ODBC_UNIXODBC_LINKER_FLAGS
            ODBC_UNIXODBC_LIBRARIES
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir_inst} ${PKG_CONFIG} odbcinst --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND env PKG_CONFIG_PATH=${_pc_dir_inst} ${PKG_CONFIG} odbcinst --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_UNIXODBC_DEFINES
            ODBC_UNIXODBC_INCLUDE_DIRS
            ODBC_UNIXODBC_COMPILER_FLAGS
            ODBC_UNIXODBC_LINKER_FLAGS
            ODBC_UNIXODBC_LIBRARIES
        )
    elseif (NOT ODBC_UNIXODBC_DIR)
        execute_process (
            COMMAND ${PKG_CONFIG} odbc --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND ${PKG_CONFIG} odbc --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_UNIXODBC_DEFINES
            ODBC_UNIXODBC_INCLUDE_DIRS
            ODBC_UNIXODBC_COMPILER_FLAGS
            ODBC_UNIXODBC_LINKER_FLAGS
            ODBC_UNIXODBC_LIBRARIES
        )

        execute_process (
            COMMAND ${PKG_CONFIG} odbcinst --libs
            OUTPUT_VARIABLE _libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process (
            COMMAND ${PKG_CONFIG} odbcinst --cflags
            OUTPUT_VARIABLE _cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        extract_flags ("${_libs} ${_cflags}"
            ODBC_UNIXODBC_DEFINES
            ODBC_UNIXODBC_INCLUDE_DIRS
            ODBC_UNIXODBC_COMPILER_FLAGS
            ODBC_UNIXODBC_LINKER_FLAGS
            ODBC_UNIXODBC_LIBRARIES
        )
    endif ()
endif ()

if (NOT ODBC_UNIXODBC_INCLUDE_DIRS)
    set (_headers sql.h;)

    foreach (_file ${_headers})
        unset (_path CACHE)
        unset (_path)

        if (ODBC_UNIXODBC_DIR)
            find_path (_path
                NAMES "${_file}"
                PATHS "${ODBC_UNIXODBC_DIR}"
                PATH_SUFFIXES "include"
                NO_DEFAULT_PATH
            )
        else ()
            find_path (_path
                NAMES "${_file}"
            )
        endif ()

        if (_path)
            list (APPEND ODBC_UNIXODBC_INCLUDE_DIRS "${_path}")
        endif ()
    endforeach ()
endif ()

if (NOT ODBC_UNIXODBC_LIBRARIES)
    set (_libs odbc;odbcinst)

    foreach (_file ${_libs})
        unset (_path CACHE)
        unset (_path)

        if (ODBC_UNIXODBC_DIR)
            find_library (_path
                NAMES "${_file}"
                PATHS "${ODBC_UNIXODBC_DIR}"
                PATH_SUFFIXES "lib"
                NO_DEFAULT_PATH
            )
        else ()
            find_library (_path
                NAMES "${_file}"
            )
        endif ()

        if (_path)
            list (APPEND ODBC_UNIXODBC_LIBRARIES "${_path}")
        endif ()
    endforeach ()
endif ()

if (ODBC_UNIXODBC_INCLUDE_DIRS AND ODBC_UNIXODBC_LIBRARIES)
    set (ODBC_UNIXODBC_FOUND TRUE)
endif ()

unset (_config CACHE)
unset (_config)

unset (_pc_dir CACHE)
unset (_pc_dir)

unset (_pc_dir_inst CACHE)
unset (_pc_dir_inst)

unset (_headers CACHE)
unset (_headers)

unset (_libs CACHE)
unset (_libs)

unset (_cflags CACHE)
unset (_cflags)

unset (_ulen CACHE)
unset (_ulen)

unset (_path CACHE)
unset (_path)

unset (_file CACHE)
unset (_file)

if (UNIX)
    include (cmake/find_ltdl.cmake)
    list (APPEND ODBC_UNIXODBC_LIBRARIES dl)
    list (APPEND ODBC_UNIXODBC_LIBRARIES "${LTDL_LIBRARY}")
endif ()

if (ODBC_UNIXODBC_DEFINES)
    list (REMOVE_DUPLICATES ODBC_UNIXODBC_DEFINES)
endif()

if (ODBC_UNIXODBC_INCLUDE_DIRS)
    list (REMOVE_DUPLICATES ODBC_UNIXODBC_INCLUDE_DIRS)
endif()

if (ODBC_UNIXODBC_COMPILER_FLAGS)
    list (REMOVE_DUPLICATES ODBC_UNIXODBC_COMPILER_FLAGS)
endif()

if (ODBC_UNIXODBC_LINKER_FLAGS)
    list (REMOVE_DUPLICATES ODBC_UNIXODBC_LINKER_FLAGS)
endif()

if (ODBC_UNIXODBC_LIBRARIES)
    list (REMOVE_DUPLICATES ODBC_UNIXODBC_LIBRARIES)
endif()

mark_as_advanced (ODBC_UNIXODBC_DEFINES)
mark_as_advanced (ODBC_UNIXODBC_INCLUDE_DIRS)
mark_as_advanced (ODBC_UNIXODBC_COMPILER_FLAGS)
mark_as_advanced (ODBC_UNIXODBC_LINKER_FLAGS)
mark_as_advanced (ODBC_UNIXODBC_LIBRARIES)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC_UNIXODBC REQUIRED_VARS ODBC_UNIXODBC_FOUND)
