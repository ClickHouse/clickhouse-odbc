set(LIBNAME _poco_net_win_ssl)

file(GLOB SRCS "src/*.cpp")

add_library(_poco_net_win_ssl ${SRCS} )
add_library(Poco::Net::WinSSL ALIAS "${LIBNAME}")

target_link_libraries(_poco_net_win_ssl Poco::Net Poco::Util Poco::Foundation Crypt32.lib)
target_include_directories(_poco_net_win_ssl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

if (OS_DARWIN OR OS_LINUX)
    target_compile_definitions (_poco_net_win_ssl PUBLIC POCO_OS_FAMILY_UNIX)
elseif(OS_WINDOWS)
    target_compile_definitions (_poco_net_win_ssl PUBLIC POCO_OS_FAMILY_WINDOWS NetSSL_Win_EXPORTS)
endif()
