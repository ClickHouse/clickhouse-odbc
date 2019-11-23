if (ODBC_FOUND)
    return ()
endif ()

#
# Consults (if present, some of) the following vars:
#     ODBC_IMPL
#
#     ODBC_DIR
#     ODBC_<IMPL>_DIR
#
#     ODBC_CONFIG
#     ODBC_<IMPL>_CONFIG
#
# Defines the following targets:
#     ODBC::API
#
# Defines (some of) the following vars:
#     ODBC_FOUND
#     ODBC_<IMPL>_FOUND
#
#     ODBC_IMPL
#
#     ODBC_DEFINES
#     ODBC_INCLUDE_DIRS
#     ODBC_COMPILER_FLAGS
#     ODBC_LINKER_FLAGS
#     ODBC_LIBRARIES
#
#     ODBC_<IMPL>_DEFINES
#     ODBC_<IMPL>_INCLUDE_DIRS
#     ODBC_<IMPL>_COMPILER_FLAGS
#     ODBC_<IMPL>_LINKER_FLAGS
#     ODBC_<IMPL>_LIBRARIES
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

    if (ODBC_DIR AND NOT ODBC_${_impl_uc}_DIR)
        set (ODBC_${_impl_uc}_DIR "${ODBC_DIR}")
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

unset (_impl)
unset (_unset_config)
unset (_unset_dir)
unset (_impls_to_try)

string (TOUPPER "${ODBC_IMPL}" _impl_uc)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ODBC REQUIRED_VARS ODBC_${_impl_uc}_FOUND)

if (ODBC_FOUND)
    message (STATUS "Using ODBC: ${ODBC_IMPL}")

    foreach (_comp DEFINES INCLUDE_DIRS COMPILER_FLAGS LIBRARIES LINKER_FLAGS)
        set (ODBC_${_comp} "${ODBC_${_impl_uc}_${_comp}}")
        mark_as_advanced (ODBC_${_comp})
        message (STATUS "\tODBC_${_comp}=${ODBC_${_comp}}")
    endforeach ()

    add_library (ODBC::API INTERFACE IMPORTED)
    target_compile_definitions (ODBC::API INTERFACE ${ODBC_DEFINES})
    target_include_directories (ODBC::API INTERFACE ${ODBC_INCLUDE_DIRS})
    target_compile_options (ODBC::API INTERFACE ${ODBC_COMPILER_FLAGS})
    if (CMAKE_VERSION VERSION_LESS "3.13.5")
        target_link_libraries (ODBC::API INTERFACE ${ODBC_LINKER_FLAGS})
    else ()
        target_link_options (ODBC::API INTERFACE ${ODBC_LINKER_FLAGS})
    endif ()
    target_link_libraries (ODBC::API INTERFACE ${ODBC_LIBRARIES})
endif ()

unset (_impl_uc)
