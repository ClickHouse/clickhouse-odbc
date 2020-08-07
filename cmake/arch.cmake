if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (ARCH_BITS 64)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (ARCH_BITS 32)
endif ()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
    set (ARCH_AARCH64 1)
endif ()

if (ARCH_AARCH64 OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set (ARCH_ARM 1)
endif ()

if (CMAKE_LIBRARY_ARCHITECTURE MATCHES "i386")
    set (ARCH_I386 1)
endif ()

if ( ( ARCH_ARM AND NOT ARCH_AARCH64 ) OR ARCH_I386)
    set (ARCH_32 1)
    message (WARNING "Support for 32bit platforms is highly experimental")
endif ()

if (CMAKE_SYSTEM MATCHES "Linux")
    set (ARCH_LINUX 1)
endif ()

if (CMAKE_SYSTEM MATCHES "FreeBSD")
    set (ARCH_FREEBSD 1)
endif ()

if (NOT MSVC)
    set (NOT_MSVC 1)
endif ()

if (NOT APPLE)
    set (NOT_APPLE 1)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (COMPILER_GCC 1)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (COMPILER_CLANG 1)
endif ()


if (ARCH_LINUX)
    if (EXISTS "/etc/os-release")
        find_program (AWK awk)

        if (AWK)
            execute_process (
                COMMAND ${AWK} "-F=" "$1==\"ID\" { gsub(/\"/, \"\", $2); print tolower($2) ;}" "/etc/os-release"
                OUTPUT_VARIABLE _os_release_id
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            if (_os_release_id STREQUAL "rhel")        
                set (UNIX_RHEL 1)
            elseif (_os_release_id STREQUAL "fedora")        
                set (UNIX_FEDORA 1)
            elseif (_os_release_id STREQUAL "centos")        
                set (UNIX_CENTOS 1)
            elseif (_os_release_id STREQUAL "debian")        
                set (UNIX_DEBIAN 1)
            elseif (_os_release_id STREQUAL "ubuntu")        
                set (UNIX_UBUNTU 1)
            endif ()
        endif ()
    endif ()
endif ()
