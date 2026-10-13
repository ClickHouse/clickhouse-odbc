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
add_contrib (zstd-cmake zstd)

if (OS_DARWIN OR OS_LINUX)
    # contrib/openssl-cmake is a verbatim copy from ClickHouse; keep it that way and
    # adapt it from here instead.
    if (OS_LINUX AND ARCH_AMD64 AND NOT OPENSSL_NO_ASM AND NOT CH_ODBC_ASM_HAS_AVXIFMA)
        # OpenSSL's x86_64 assembly uses AVX-IFMA (`{vex} vpmadd52luq`), which needs
        # LLVM >= 16 or GNU binutils >= 2.40.
        enable_language (ASM)
        set (_probe "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/avxifma_probe.s")
        file (WRITE "${_probe}" ".text\n{vex} vpmadd52luq 0(%rsi),%ymm1,%ymm3\n")
        set (CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        try_compile (CH_ODBC_ASM_HAS_AVXIFMA "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/avxifma_probe"
            SOURCES "${_probe}" OUTPUT_VARIABLE _probe_output)
        unset (CMAKE_TRY_COMPILE_TARGET_TYPE)
        if (NOT CH_ODBC_ASM_HAS_AVXIFMA)
            unset (CH_ODBC_ASM_HAS_AVXIFMA CACHE)
            message (FATAL_ERROR
                "The assembler (CMAKE_ASM_COMPILER=${CMAKE_ASM_COMPILER}) cannot assemble the AVX-IFMA "
                "instructions used by OpenSSL. Use Clang >= 16 or GNU binutils >= 2.40, or pass "
                "-DCMAKE_ASM_COMPILER=<clang >= 16> (works with GCC as the C/C++ compiler), or "
                "build the portable C implementation with -DOPENSSL_NO_ASM=ON (much slower, no AES-NI, "
                "not constant-time; not for release builds).\n"
                "Probe output:\n${_probe_output}")
        endif ()
        message (STATUS "Assembler supports AVX-IFMA: yes")
        unset (_probe)
        unset (_probe_output)
    endif ()
    set (ClickHouse_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
    add_contrib (openssl-cmake openssl)
    unset (ClickHouse_SOURCE_DIR)
    if (OS_LINUX)
        # The pre-generated assembly has no .note.GNU-stack section; without this,
        # older linkers (e.g. binutils 2.38) mark the driver as needing an executable stack.
        target_compile_options (crypto PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wa,--noexecstack>)
    endif ()
endif()

add_contrib (zlib-ng-cmake zlib-ng)

if (CH_ODBC_ENABLE_TESTING)
    # The driver always uses the static CRT on MSVC; make Google Test match it.
    set (gtest_force_shared_crt OFF CACHE INTERNAL "")
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

# Some of the above may define this target.
if (TARGET uninstall AND CMAKE_FOLDER)
    set_target_properties (uninstall PROPERTIES FOLDER "${CMAKE_FOLDER}")
endif ()

# Some of the above may set a cache entry that will interfere with later reconfigurations.
unset (ODBC_CONFIG CACHE)
