
# /usr/bin/x86_64-linux-gnu-ld: /usr/lib/gcc/x86_64-linux-gnu/8/../../../x86_64-linux-gnu/libltdl.a(libltdl_libltdl_la-lt__alloc.o): relocation R_X86_64_PC32 against symbol `stderr@@GLIBC_2.2.5' can not be used when making a shared object; recompile with -fPIC
if (NOT BUILD_SHARED)
    execute_process(
        COMMAND lsb_release -is
        OUTPUT_VARIABLE _distname
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (_distname)
        message (STATUS "lsb_release -is: ${_distname}")
    else ()
        execute_process(
            COMMAND sh -c "head -n 1 /etc/issue | cut -f 1 -d ' '"
            OUTPUT_VARIABLE _distname
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (_distname)
            message (STATUS "Distro name from /etc/issue: ${_distname}")
        endif
    endif

    if (_distname STREQUAL "Ubuntu" OR _distname STREQUAL "Debian")
        list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
        set (_suffixes_reversed 1)
        message (STATUS "Trying to find shared version of ltdl library because linking with libltdl.a is broken in ${_distname}")
    endif ()
endif ()

set (LTDL_PATHS "/usr/local/opt/libtool/lib")
find_library (LTDL_LIBRARY ltdl PATHS ${LTDL_PATHS})
message (STATUS "Using ltdl: ${LTDL_LIBRARY}")

if (_suffixes_reversed)
    list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
endif ()
