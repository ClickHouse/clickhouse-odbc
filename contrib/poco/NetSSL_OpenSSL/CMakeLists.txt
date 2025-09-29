# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(NetSSL ${SRCS})
add_library(Poco::NetSSL ALIAS NetSSL)

target_link_libraries(NetSSL PUBLIC Poco::Crypto Poco::Util Poco::Net)
target_include_directories(NetSSL
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC AND OPENSSL_USE_STATIC_LIBS)
	target_link_libraries(NetSSL PUBLIC ws2_32.lib Crypt32.lib)
endif()

