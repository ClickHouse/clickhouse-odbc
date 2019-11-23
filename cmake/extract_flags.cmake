macro (extract_flags
    _mixed_flags_val
    _defines_name
    _include_dirs_name
    _compiler_flags_name
    _linker_flags_name
    _libraries_name
)
    string (REPLACE " " ";" _mixed_flags "${_mixed_flags_val}")

    foreach(_flag ${_mixed_flags})
        if ("${_flag}" MATCHES "^-D(.*)$")
            list (APPEND ${_defines_name} "${CMAKE_MATCH_1}")
        elseif ("${_flag}" MATCHES "^-I(.*)$")
            list (APPEND ${_include_dirs_name} "${CMAKE_MATCH_1}")
        elseif ("${_flag}" MATCHES "^-L(.*)$")
            list (APPEND ${_linker_flags_name} "-L${CMAKE_MATCH_1}")
        elseif ("${_flag}" MATCHES "^-l(.*)$")
            list (APPEND ${_libraries_name} "${CMAKE_MATCH_1}")
        else ()
            list (APPEND ${_compiler_flags_name} "${_flag}")
        endif()
    endforeach()

    unset (_flag)
    unset (_mixed_flags)
endmacro()
