
if (OS_DARWIN OR OS_LINIX)
    if (NOT TARGET ch_contrib::unixodbc)
        message(FATAL_ERROR "Configuration error: unixodbc is not a target")
    endif()

    set (LIBRARY_DIR "${CMAKE_SOURCE_DIR}/contrib/nanodbc")

    set (SRCS
        "${LIBRARY_DIR}/nanodbc/nanodbc.cpp"
    )

    add_library(_nanodbc ${SRCS})
    target_link_libraries(_nanodbc PUBLIC ch_contrib::unixodbc)
    target_include_directories(_nanodbc SYSTEM PUBLIC "${LIBRARY_DIR}/")
    add_library(ch_contrib::nanodbc ALIAS _nanodbc)
elseif (OS_WINDOWS)
    set (LIBRARY_DIR "${CMAKE_SOURCE_DIR}/contrib/nanodbc")
    set (SRCS
        "${LIBRARY_DIR}/nanodbc/nanodbc.cpp"
    )

    add_library(_nanodbc ${SRCS})
    target_link_libraries(_nanodbc PUBLIC ODBC::Driver)
    target_include_directories(_nanodbc SYSTEM PUBLIC "${LIBRARY_DIR}/")
    add_library(ch_contrib::nanodbc ALIAS _nanodbc)

endif()
