#
# Finds and exposes libraries, include dirs, compiler flags, etc. for ODBC implementations (referred as "providers"),
# as well as defines imported interface libraries that bring the required config for driver or application linkage.
# Delegates the main job of detection to corresponding sub-Find* modules: UnixODBC, iODBC, and MDAC.
#
# Consults the following vars (if set):
#     ODBC_PROVIDER - one of UnixODBC, iODBC, or MDAC
#
#     ODBC_DIR
#     ODBC_CONFIG_SCRIPT
#
#     ODBC_SKIP_CONFIG_SCRIPT
#     ODBC_SKIP_PKG_CONFIG
#     ODBC_SKIP_BREW
#
#     ODBC_<PROVIDER>_DIR
#     ODBC_<PROVIDER>_CONFIG_SCRIPT
#
#     ODBC_<PROVIDER>_SKIP_CONFIG_SCRIPT
#     ODBC_<PROVIDER>_SKIP_PKG_CONFIG
#     ODBC_<PROVIDER>_SKIP_BREW
#
# Defines the following targets:
#     ODBC::App
#     ODBC::Driver
#
# Defines (some of) the following vars:
#     ODBC_FOUND
#     ODBC_PROVIDER
#
#     ODBC_DIR
#     ODBC_CONFIG_SCRIPT
#
#     ODBC_<PROVIDER>_FOUND
#     ODBC_<PROVIDER>
#
#     ODBC_<PROVIDER>_DIR
#     ODBC_<PROVIDER>_CONFIG_SCRIPT
#
#     ODBC_UNIXODBC_ISQL
#     ODBC_UNIXODBC_IUSQL
#
#     ODBC_IODBC_IODBCTEST
#     ODBC_IODBC_IODBCTESTW
#
#     ODBC_APP_DEFINES
#     ODBC_APP_INCLUDE_DIRS
#     ODBC_APP_COMPILER_FLAGS
#     ODBC_APP_LINKER_FLAGS
#
#     ODBC_DRIVER_DEFINES
#     ODBC_DRIVER_INCLUDE_DIRS
#     ODBC_DRIVER_COMPILER_FLAGS
#     ODBC_DRIVER_LINKER_FLAGS
#
#     ODBC_<PROVIDER>_APP_DEFINES
#     ODBC_<PROVIDER>_APP_INCLUDE_DIRS
#     ODBC_<PROVIDER>_APP_COMPILER_FLAGS
#     ODBC_<PROVIDER>_APP_LINKER_FLAGS
#
#     ODBC_<PROVIDER>_DRIVER_DEFINES
#     ODBC_<PROVIDER>_DRIVER_INCLUDE_DIRS
#     ODBC_<PROVIDER>_DRIVER_COMPILER_FLAGS
#     ODBC_<PROVIDER>_DRIVER_LINKER_FLAGS
#

if (ODBC_PROVIDER)
    string (TOUPPER "${ODBC_PROVIDER}" _provider_uc)
    if ("${_provider_uc}" STREQUAL "MDAC")
        set (_providers_to_try MDAC;)
    elseif ("${_provider_uc}" STREQUAL "UNIXODBC")
        set (_providers_to_try UnixODBC;)
    elseif ("${_provider_uc}" STREQUAL "IODBC")
        set (_providers_to_try iODBC;)
    else ()
        message (FATAL_ERROR "Unknown ODBC provider: ${ODBC_PROVIDER}")
    endif ()
# Platform-default provides:
elseif (WIN32)
    set (_providers_to_try MDAC;)
elseif (APPLE)
    set (_providers_to_try iODBC;UnixODBC)
else ()
    set (_providers_to_try UnixODBC;iODBC)
endif ()

foreach (_provider ${_providers_to_try})
    string (TOUPPER "${_provider}" _provider_uc)

    unset (_unset_dir)
    unset (_unset_config_script)
    unset (_unset_skip_config_script)
    unset (_unset_skip_pkg_config)
    unset (_unset_skip_brew)

    if (ODBC_DIR AND NOT ODBC_${_provider_uc}_DIR)
        set (ODBC_${_provider_uc}_DIR "${ODBC_DIR}")
        set (_unset_dir TRUE)
    endif ()

    if (ODBC_CONFIG_SCRIPT AND NOT ODBC_${_provider_uc}_CONFIG_SCRIPT)
        set (ODBC_${_provider_uc}_CONFIG_SCRIPT "${ODBC_CONFIG_SCRIPT}")
        set (_unset_config_script TRUE)
    endif ()

    if (ODBC_SKIP_CONFIG_SCRIPT AND NOT ODBC_${_provider_uc}_SKIP_CONFIG_SCRIPT)
        set (ODBC_${_provider_uc}_SKIP_CONFIG_SCRIPT "${ODBC_SKIP_CONFIG_SCRIPT}")
        set (_unset_skip_config TRUE)
    endif ()

    if (ODBC_SKIP_PKG_CONFIG AND NOT ODBC_${_provider_uc}_SKIP_PKG_CONFIG)
        set (ODBC_${_provider_uc}_SKIP_PKG_CONFIG "${ODBC_SKIP_PKG_CONFIG}")
        set (_unset_skip_pkg_config TRUE)
    endif ()

    if (ODBC_SKIP_BREW AND NOT ODBC_${_provider_uc}_SKIP_BREW)
        set (ODBC_${_provider_uc}_SKIP_BREW "${ODBC_SKIP_BREW}")
        set (_unset_skip_brew TRUE)
    endif ()

    find_package (${_provider})

    if (_unset_skip_brew)
        unset (ODBC_${_provider_uc}_SKIP_BREW)
    endif ()

    if (_unset_skip_pkg_config)
        unset (ODBC_${_provider_uc}_SKIP_PKG_CONFIG)
    endif ()

    if (_unset_skip_config_script)
        unset (ODBC_${_provider_uc}_SKIP_CONFIG_SCRIPT)
    endif ()

    if (ODBC_${_provider_uc}_FOUND)
        if (ODBC_${_provider_uc}_CONFIG_SCRIPT)
            set (ODBC_CONFIG_SCRIPT "${ODBC_${_provider_uc}_CONFIG_SCRIPT}")
        endif ()

        if (ODBC_${_provider_uc}_DIR)
            set (ODBC_DIR "${ODBC_${_provider_uc}_DIR}")
        endif ()

        set (ODBC_PROVIDER "${_provider}")
        set (ODBC_${_provider_uc} TRUE)

        break ()
    else ()
        if (_unset_config_script)
            unset (ODBC_${_provider_uc}_CONFIG_SCRIPT)
        endif ()

        if (_unset_dir)
            unset (ODBC_${_provider_uc}_DIR)
        endif ()
    endif ()
endforeach ()

unset (_unset_dir)
unset (_unset_config_script)
unset (_unset_skip_config_script)
unset (_unset_skip_pkg_config)
unset (_unset_skip_brew)
unset (_provider)
unset (_providers_to_try)

string (TOUPPER "${ODBC_PROVIDER}" _provider_uc)

if (ODBC_PROVIDER AND ODBC_${_provider_uc}_FOUND)
    message (STATUS "Using ODBC: ${ODBC_PROVIDER}")

    if (NOT "${_provider_uc}" STREQUAL "MDAC")
        message (STATUS "\tODBC_DIR=${ODBC_DIR}")
        message (STATUS "\tODBC_CONFIG_SCRIPT=${ODBC_CONFIG_SCRIPT}")
    endif ()

    if ("${_provider_uc}" STREQUAL "UNIXODBC")
        message (STATUS "\tODBC_UNIXODBC_ISQL=${ODBC_UNIXODBC_ISQL}")
        message (STATUS "\tODBC_UNIXODBC_IUSQL=${ODBC_UNIXODBC_IUSQL}")
    elseif ("${_provider_uc}" STREQUAL "IODBC")
        message (STATUS "\tODBC_IODBC_IODBCTEST=${ODBC_IODBC_IODBCTEST}")
        message (STATUS "\tODBC_IODBC_IODBCTESTW=${ODBC_IODBC_IODBCTESTW}")
    endif ()

    foreach (_role App Driver)
        string (TOUPPER "${_role}" _role_uc)

        foreach (_comp_uc DEFINES INCLUDE_DIRS COMPILER_FLAGS LINKER_FLAGS)
            unset (ODBC_${_role_uc}_${_comp_uc})
            if (ODBC_${_provider_uc}_${_role_uc}_${_comp_uc})
                set (ODBC_${_role_uc}_${_comp_uc} "${ODBC_${_provider_uc}_${_role_uc}_${_comp_uc}}")
                mark_as_advanced (ODBC_${_role_uc}_${_comp_uc})
            endif ()
            message (STATUS "\tODBC_${_role_uc}_${_comp_uc}=${ODBC_${_role_uc}_${_comp_uc}}")
        endforeach ()

        unset (_comp_uc)

        add_library (ODBC::${_role} INTERFACE IMPORTED)
        target_compile_definitions (ODBC::${_role} INTERFACE ${ODBC_${_role_uc}_DEFINES})
        target_include_directories (ODBC::${_role} INTERFACE ${ODBC_${_role_uc}_INCLUDE_DIRS})
        target_compile_options (ODBC::${_role} INTERFACE ${ODBC_${_role_uc}_COMPILER_FLAGS})
        if (CMAKE_VERSION VERSION_LESS "3.13.5")
            target_link_libraries (ODBC::${_role} INTERFACE ${ODBC_${_role_uc}_LINKER_FLAGS})
        else ()
            target_link_options (ODBC::${_role} INTERFACE ${ODBC_${_role_uc}_LINKER_FLAGS})
        endif ()

        unset (_role_uc)
    endforeach ()

    unset (_role)
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC REQUIRED_VARS ODBC_${_provider_uc}_FOUND)

unset (_provider_uc)
