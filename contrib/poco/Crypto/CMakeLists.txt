# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(Crypto ${SRCS})
add_library(Poco::Crypto ALIAS Crypto)

target_link_libraries(Crypto PUBLIC Poco::Foundation OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(Crypto
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
