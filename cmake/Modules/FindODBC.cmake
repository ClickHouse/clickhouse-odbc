if (ODBC_FOUND)
    return ()
endif ()

#
# Consults (if present, some of) the following vars:
#     ODBC_IMPL
#
#     ODBC_IMPL_DIR
#     ODBC_<IMPL>_DIR
#
#     ODBC_CONFIG
#     ODBC_<IMPL>_CONFIG
#
# Defines the following targets:
#     ODBC::ClientAPI
#     ODBC::DriverAPI
#
# Defines (some of) the following vars:
#     ODBC_FOUND
#     ODBC_<IMPL>_FOUND
#
#     ODBC_IMPL
#
#     ODBC_CONFIG
#     ODBC_<IMPL>_CONFIG
#
#     ODBC_CLIENT_DEFINES
#     ODBC_CLIENT_INCLUDE_DIRS
#     ODBC_CLIENT_COMPILER_FLAGS
#     ODBC_CLIENT_LINKER_FLAGS
#     ODBC_CLIENT_LIBRARIES
#
#     ODBC_<IMPL>_CLIENT_DEFINES
#     ODBC_<IMPL>_CLIENT_INCLUDE_DIRS
#     ODBC_<IMPL>_CLIENT_COMPILER_FLAGS
#     ODBC_<IMPL>_CLIENT_LINKER_FLAGS
#     ODBC_<IMPL>_CLIENT_LIBRARIES
#
#     ODBC_DRIVER_DEFINES
#     ODBC_DRIVER_INCLUDE_DIRS
#     ODBC_DRIVER_COMPILER_FLAGS
#     ODBC_DRIVER_LINKER_FLAGS
#     ODBC_DRIVER_LIBRARIES
#
#     ODBC_<IMPL>_DRIVER_DEFINES
#     ODBC_<IMPL>_DRIVER_INCLUDE_DIRS
#     ODBC_<IMPL>_DRIVER_COMPILER_FLAGS
#     ODBC_<IMPL>_DRIVER_LINKER_FLAGS
#     ODBC_<IMPL>_DRIVER_LIBRARIES
#

if (WIN32)
    set (_impls_to_try MDAC)
elseif (APPLE)
    set (_impls_to_try iODBC;UnixODBC)
else ()
    set (_impls_to_try UnixODBC;iODBC)
endif ()

if (ODBC_IMPL)
    string (TOUPPER "${ODBC_IMPL}" _impl_uc)
    if ("${_impl_uc}" STREQUAL "MDAC")
        set (ODBC_IMPL MDAC)
    elseif ("${_impl_uc}" STREQUAL "UNIXODBC")
        set (ODBC_IMPL UnixODBC)
    elseif ("${_impl_uc}" STREQUAL "IODBC")
        set (ODBC_IMPL iODBC)
    else ()
        message (FATAL_ERROR "Unknown ODBC implementation: ${ODBC_IMPL}")
    endif()
    set (_impls_to_try "${ODBC_IMPL}")
endif ()

foreach (_impl ${_impls_to_try})
    string (TOUPPER "${_impl}" _impl_uc)

    if (ODBC_IMPL_DIR AND NOT ODBC_${_impl_uc}_DIR)
        set (ODBC_${_impl_uc}_DIR "${ODBC_IMPL_DIR}")
        set (_unset_dir TRUE)
    endif ()
    
    if (ODBC_CONFIG AND NOT ODBC_${_impl_uc}_CONFIG)
        set (ODBC_${_impl_uc}_CONFIG "${ODBC_CONFIG}")
        set (_unset_config TRUE)
    endif ()

    find_package (${_impl})
    
    if (ODBC_${_impl_uc}_FOUND OR NOT ODBC_IMPL)
        set (ODBC_IMPL "${_impl}")
    endif ()

    if (ODBC_${_impl_uc}_FOUND)
        break ()
    else ()
        if (_unset_config)
            unset (ODBC_${_impl_uc}_CONFIG)
            unset (_unset_config)
        endif ()

        if (_unset_dir)
            unset (ODBC_${_impl_uc}_DIR)
            unset (_unset_dir)
        endif ()
    endif ()
endforeach ()

unset (_unset_config)
unset (_unset_dir)
unset (_impl)
unset (_impl_uc)
unset (_impls_to_try)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC REQUIRED_VARS ODBC_${ODBC_IMPL}_FOUND)

if (ODBC_FOUND)
    message (STATUS "Using ODBC: ${ODBC_IMPL}")

    string (TOUPPER "${ODBC_IMPL}" _impl_uc)

    if (ODBC_${_impl_uc}_CONFIG AND NOT ODBC_CONFIG)
        set (ODBC_CONFIG "${ODBC_${_impl_uc}_CONFIG}")
    endif ()

    if (NOT "${ODBC_CONFIG}" STREQUAL "")
        message (STATUS "\tODBC_CONFIG=${ODBC_CONFIG}")
    endif ()

    mark_as_advanced (ODBC_CONFIG)

    foreach (_api CLIENT DRIVER)
        foreach (_comp DEFINES INCLUDE_DIRS COMPILER_FLAGS LIBRARIES LINKER_FLAGS)
            set (ODBC_${_api}_${_comp} "${ODBC_${_impl_uc}_${_api}_${_comp}}")
            mark_as_advanced (ODBC_${_api}_${_comp})
            message (STATUS "\tODBC_${_api}_${_comp}=${ODBC_${_api}_${_comp}}")
        endforeach ()
    endforeach ()

    unset (_impl_uc)

    add_library (ODBC::ClientAPI INTERFACE IMPORTED)
    target_compile_definitions (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_DEFINES})
    target_include_directories (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_INCLUDE_DIRS})
    target_compile_options (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_COMPILER_FLAGS})
    if (CMAKE_VERSION VERSION_LESS "3.13.5")
        target_link_libraries (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_LINKER_FLAGS})
    else ()
        target_link_options (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_LINKER_FLAGS})
    endif ()
    target_link_libraries (ODBC::ClientAPI INTERFACE ${ODBC_CLIENT_LIBRARIES})

    add_library (ODBC::DriverAPI INTERFACE IMPORTED)
    target_compile_definitions (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_DEFINES})
    target_include_directories (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_INCLUDE_DIRS})
    target_compile_options (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_COMPILER_FLAGS})
    if (CMAKE_VERSION VERSION_LESS "3.13.5")
        target_link_libraries (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_LINKER_FLAGS})
    else ()
        target_link_options (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_LINKER_FLAGS})
    endif ()
    target_link_libraries (ODBC::DriverAPI INTERFACE ${ODBC_DRIVER_LIBRARIES})
endif ()
