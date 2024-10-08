get_filename_component (_current_dir_name "${CMAKE_CURRENT_LIST_DIR}" NAME)
if (CMAKE_FOLDER)
    set (CMAKE_FOLDER "$CMAKE_FOLDER/${_current_dir_name}")
else ()
    set (CMAKE_FOLDER "${_current_dir_name}")
endif ()
unset (_current_dir_name)

set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL 1)

# add_contrib cmake_folder[ base_folder1[, ...base_folderN]]
function(add_contrib cmake_folder)
    if (ARGN)
        set(base_folders ${ARGN})
    else()
        set(base_folders ${cmake_folder})
    endif()

    foreach (base_folder ${base_folders})
        # some typos in the code
        if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${base_folder}")
            message(FATAL_ERROR "No such base folder '${base_folder}' (for '${cmake_folder}' cmake folder). Typo in the base folder name?")
        endif()

        file(GLOB contrib_files "${base_folder}/*")
        if (NOT contrib_files)
            # Checking out *all* submodules takes > 5 min. Therefore, the smoke build ("FastTest") in CI initializes only the set of
            # submodules minimally needed for a build and we cannot assume here that all submodules are populated.
            message(STATUS "submodule ${base_folder} is missing or empty. to fix try run:")
            message(STATUS "    git submodule update --init")
            return()
        endif()
    endforeach()

    message(STATUS "Adding contrib module ${base_folders} (configuring with ${cmake_folder})")
    add_subdirectory (${cmake_folder})
endfunction()

add_contrib (icu-cmake icu)
add_contrib (unixodbc-cmake unixodbc)
add_contrib (nanodbc-cmake nanodbc)
add_contrib (lz4-cmake lz4)
add_contrib (double-conversion-cmake double-conversion)

if (OS_DARWIN OR OS_LINUX)
    add_contrib (openssl-cmake openssl)
endif()

add_contrib (zlib-ng-cmake zlib-ng)
set (FOLLY_INLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/folly" CACHE INTERNAL "")

if (CH_ODBC_ENABLE_TESTING)
    if (CH_ODBC_PREFER_BUNDLED_GOOGLETEST OR NOT GTEST_FOUND)
        if (NOT GTEST_FOUND AND NOT CH_ODBC_PREFER_BUNDLED_GOOGLETEST)
            message (WARNING "Google Test: unable to find system Google Test, falling back to using the bundled variant of the library")
        endif ()

        if (NOT CH_ODBC_RUNTIME_LINK_STATIC)
            set (gtest_force_shared_crt ON CACHE INTERNAL "")
        endif ()

        set (gtest_hide_internal_symbols ON CACHE INTERNAL "")
        set (BUILD_GMOCK   ON CACHE BOOL "")
        set (INSTALL_GTEST OFF CACHE BOOL "")

        add_subdirectory (googletest)

        mark_as_advanced (BUILD_GMOCK)
        mark_as_advanced (INSTALL_GTEST)

        set_target_properties (gtest      PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gtest_main PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gmock      PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gmock_main PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
    endif ()
endif ()

# Some of the above may define this target.
if (TARGET uninstall AND CMAKE_FOLDER)
    set_target_properties (uninstall PROPERTIES FOLDER "${CMAKE_FOLDER}")
endif ()

# Some of the above may set a cache entry that will interfere with later reconfigurations.
unset (ODBC_CONFIG CACHE)
