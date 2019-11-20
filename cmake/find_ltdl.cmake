
# /usr/bin/x86_64-linux-gnu-ld: /usr/lib/gcc/x86_64-linux-gnu/8/../../../x86_64-linux-gnu/libltdl.a(libltdl_libltdl_la-lt__alloc.o): relocation R_X86_64_PC32 against symbol `stderr@@GLIBC_2.2.5' can not be used when making a shared object; recompile with -fPIC
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    # using `/etc/debian_version` rather that `lsb_release -is` since some Ubuntu or Debian-based disros (hello KDE Neon)
    # have different distribution ID, but `/etc/debian_version` MUST be present in all Debian-based disros.
    execute_process(
        COMMAND cat /etc/debian_version
        OUTPUT_VARIABLE _debian_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (_debian_version)
        list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
        set (_suffixes_reversed 1)
        message (STATUS "Trying to find shared version of ltdl library because linking with libltdl.a is broken in Debian-based distros")
    endif ()
endif ()

set (LTDL_PATHS "/usr/local/opt/libtool/lib")
find_library (LTDL_LIBRARY ltdl PATHS ${LTDL_PATHS})
message (STATUS "Using ltdl: ${LTDL_LIBRARY}")

if (_suffixes_reversed)
    list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
endif ()
