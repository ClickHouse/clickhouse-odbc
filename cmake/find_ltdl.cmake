
# /usr/bin/x86_64-linux-gnu-ld: /usr/lib/gcc/x86_64-linux-gnu/8/../../../x86_64-linux-gnu/libltdl.a(libltdl_libltdl_la-lt__alloc.o): relocation R_X86_64_PC32 against symbol `stderr@@GLIBC_2.2.5' can not be used when making a shared object; recompile with -fPIC
if (NOT BUILD_SHARED)
    execute_process(COMMAND lsb_release -cs OUTPUT_VARIABLE _codename OUTPUT_STRIP_TRAILING_WHITESPACE)
    message (STATUS "lsb_release -cs: ${_codename}")
    if (_codename STREQUAL "bionic" OR _codename STREQUAL "xenial" OR _codename STREQUAL "trusty" OR _codename STREQUAL "stretch" OR _codename STREQUAL "cosmic" OR _codename STREQUAL "disco")
        list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
        set (_suffixes_reversed 1)
        message (STATUS "Trying to find shared version of ltdl library because linking with libltdl.a is broken in ${_codename}")
    endif ()
endif ()

set (LTDL_PATHS "/usr/local/opt/libtool/lib")
find_library (LTDL_LIBRARY ltdl PATHS ${LTDL_PATHS})
message (STATUS "Using ltdl: ${LTDL_LIBRARY}")

if (_suffixes_reversed)
    list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
endif ()
