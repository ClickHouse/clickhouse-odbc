# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

# Platform Specific
POCO_SOURCES_AUTO_PLAT(SRCS UNIX src/SyslogChannel.cpp)
POCO_HEADERS_AUTO(SRCS include/Poco/SyslogChannel.h)

POCO_SOURCES_AUTO_PLAT(SRCS WIN32
	src/WindowsConsoleChannel.cpp
	src/EventLogChannel.cpp
	)

# Messages
POCO_MESSAGES(SRCS Logging src/pocomsg.mc)

# pcre2
POCO_SOURCES(SRCS pcre2
    src/pcre2_auto_possess.c
    src/pcre2_chartables.c
    src/pcre2_chkdint.c
    src/pcre2_compile.c
    src/pcre2_config.c
    src/pcre2_context.c
    src/pcre2_convert.c
    src/pcre2_dfa_match.c
    src/pcre2_error.c
    src/pcre2_extuni.c
    src/pcre2_find_bracket.c
    src/pcre2_jit_compile.c
    src/pcre2_maketables.c
    src/pcre2_match.c
    src/pcre2_match_data.c
    src/pcre2_newline.c
    src/pcre2_ord2utf.c
    src/pcre2_pattern_info.c
    src/pcre2_script_run.c
    src/pcre2_serialize.c
    src/pcre2_string_utils.c
    src/pcre2_study.c
    src/pcre2_substitute.c
    src/pcre2_substring.c
    src/pcre2_tables.c
    src/pcre2_ucd.c
    src/pcre2_valid_utf.c
    src/pcre2_xclass.c
)

POCO_SOURCES(SRCS zlib
    src/adler32.c
    src/compress.c
    src/crc32.c
    src/deflate.c
    src/infback.c
    src/inffast.c
    src/inflate.c
    src/inftrees.c
    src/trees.c
    src/zutil.c
)

POCO_SOURCES(SRCS utf8proc
    src/utf8proc.c
)

add_library(Foundation ${SRCS})
add_library(Poco::Foundation ALIAS Foundation)
target_compile_definitions(Foundation PUBLIC UTF8PROC_STATIC)
target_compile_definitions(Foundation PUBLIC POCO_CMAKE)

target_compile_definitions(Foundation
	PUBLIC
		$<$<CONFIG:Debug>:_DEBUG>
)

target_include_directories(Foundation
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Foundation PUBLIC POCO_STATIC)

if(WIN32)
	target_compile_definitions(Foundation PUBLIC POCO_OS_FAMILY_WINDOWS)
    target_compile_definitions(Foundation PUBLIC POCO_NO_AUTOMATIC_LIBS)
	target_link_libraries(Foundation PUBLIC iphlpapi)
endif()

if(UNIX)
    target_compile_definitions(Foundation PUBLIC POCO_OS_FAMILY_UNIX)
    if(APPLE)
        target_compile_definitions(Foundation PUBLIC POCO_HAVE_IPv6 POCO_NO_STAT64)
        target_link_libraries(Foundation PUBLIC ${CMAKE_DL_LIBS})
    else()
        target_compile_definitions(Foundation PUBLIC _REENTRANT _THREAD_SAFE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
        target_compile_definitions(Foundation PUBLIC _XOPEN_SOURCE=500 POCO_HAVE_FD_EPOLL)
        target_link_libraries(Foundation PUBLIC pthread ${CMAKE_DL_LIBS} rt)
    endif(APPLE)
endif(UNIX)
