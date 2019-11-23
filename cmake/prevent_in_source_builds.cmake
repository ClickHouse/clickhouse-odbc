foreach(_source_dir "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    if ("${_source_dir}" STREQUAL "")
        continue ()
    endif ()

    get_filename_component (_source_dir_realpath "${_source_dir}" REALPATH)

    foreach(_binary_dir "${CMAKE_BINARY_DIR}" "${PROJECT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        if ("${_binary_dir}" STREQUAL "")
            continue ()
        endif ()

        get_filename_component (_binary_dir_realpath "${_binary_dir}" REALPATH)

        set (_prev_binary_dir_realpath)
        set (_binary_dir_name)

        while (NOT "${_binary_dir_realpath}" STREQUAL "${_prev_binary_dir_realpath}")
            if (
                "${_binary_dir_realpath}" STREQUAL "${_source_dir_realpath}" AND
                NOT "${_binary_dir_name}" MATCHES "^build([-_].*)?\$"
            )
                message (FATAL_ERROR "In-source builds are not allowed:
    CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}
    CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}
    PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}
    PROJECT_BINARY_DIR = ${PROJECT_BINARY_DIR}
    CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}
    CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}
")
            endif ()

            get_filename_component (_binary_dir_name "${_binary_dir_realpath}" NAME)
            set (_prev_binary_dir_realpath "${_binary_dir_realpath}")
            get_filename_component (_binary_dir_realpath "${_binary_dir_realpath}" DIRECTORY)
        endwhile ()
    endforeach()
endforeach()

unset (_binary_dir_name)
unset (_prev_binary_dir_realpath)
unset (_binary_dir_realpath)
unset (_binary_dir)
unset (_source_dir_realpath)
unset (_source_dir)
