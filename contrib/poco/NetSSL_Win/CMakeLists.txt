# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(NetSSLWin ${SRCS})
set_target_properties(NetSSLWin PROPERTIES CXX_STANDARD 17)
add_library(Poco::NetSSLWin ALIAS NetSSLWin)

target_link_libraries(NetSSLWin PUBLIC Poco::Net Poco::Util Crypt32.lib)
target_include_directories(NetSSLWin
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
