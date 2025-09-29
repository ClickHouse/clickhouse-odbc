# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Platform Specific
POCO_SOURCES_AUTO_PLAT(SRCS WIN32 src/wepoll.c)
POCO_HEADERS_AUTO(SRCS src/wepoll.h)

if (MSVC OR MINGW)
	set(HAVE_SENDFILE ON)
else()
	include(CheckIncludeFiles)
	include(CheckSymbolExists)
	check_include_files(sys/sendfile.h HAVE_SYS_SENDFILE_H)
	if(HAVE_SYS_SENDFILE_H)
		check_symbol_exists(sendfile sys/sendfile.h HAVE_SENDFILE)
		if (NOT HAVE_SENDFILE)
			check_symbol_exists(sendfile64 sys/sendfile.h HAVE_SENDFILE64)
		endif()
	else()
		# BSD version
		check_symbol_exists(sendfile "sys/types.h;sys/socket.h;sys/uio.h" HAVE_SENDFILE)
	endif()
endif()

# Version Resource
if(MSVC AND BUILD_SHARED_LIBS)
    source_group("Resources" FILES ${POCO_SOURCE_ROOT}/DLLVersion.rc)
	list(APPEND SRCS ${POCO_SOURCE_ROOT}/DLLVersion.rc)
endif()

add_library(Net ${SRCS})
add_library(Poco::Net ALIAS Net)

target_link_libraries(Net PUBLIC Poco::Foundation)
# Windows need additional libraries
if(WIN32)
	target_link_libraries(Net PUBLIC "iphlpapi.lib")
	target_link_libraries(Net PUBLIC "ws2_32.lib")
	if (MINGW)
		target_link_libraries(Net PUBLIC "mswsock.lib")
	endif()
endif(WIN32)

target_include_directories(Net
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
