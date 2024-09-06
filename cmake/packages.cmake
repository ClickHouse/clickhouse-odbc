if (OS_LINUX)
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
