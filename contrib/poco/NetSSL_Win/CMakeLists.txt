set(LIBNAME "NetSSLWin")
set(POCO_LIBNAME "Poco${LIBNAME}")

# Sources
file(GLOB SRCS "src/*.cpp")

# Headers
#file(GLOB_RECURSE HDRS "include/*.h" )

# Version Resource
#if(MSVC AND NOT POCO_STATIC)
#    source_group("Resources" FILES ${CMAKE_SOURCE_DIR}/DLLVersion.rc)
#    list(APPEND SRCS ${CMAKE_SOURCE_DIR}/DLLVersion.rc)
#endif()

add_library( "${LIBNAME}" ${LIB_MODE} ${SRCS} )
add_library(Poco::Net::WinSSL ALIAS "${LIBNAME}")
set_target_properties( "${LIBNAME}"
    PROPERTIES
    VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
    OUTPUT_NAME ${POCO_LIBNAME}
    DEFINE_SYMBOL NetSSL_Win_EXPORTS
    )

target_link_libraries( "${LIBNAME}" Net Util Foundation Crypt32.lib )
target_include_directories( "${LIBNAME}"
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
target_compile_definitions("${LIBNAME}" PUBLIC ${LIB_MODE_DEFINITIONS})


