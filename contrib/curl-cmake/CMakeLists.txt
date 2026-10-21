# libcurl, built from the unmodified upstream sources (contrib/curl).
#
# curl's own CMake is not used: it probes the build machine with hundreds of configure checks,
# needs workarounds to find the bundled OpenSSL, and adds many cache options. Instead, the
# configuration is fixed in config/curl_config.h: HTTP and HTTPS only, no optional dependencies,
# the bundled OpenSSL on Linux/macOS and the native Schannel on Windows. Content decoding (zstd) is
# done by the driver itself, see the motivation at HttpResponse::Zstd in driver/http_client.cpp.
#
# When updating curl:
#  - The source list is taken from curl's lib/Makefile.inc, so new or removed files need no change
#    here.
#  - config/curl_config.h has to be checked against what curl's own CMake generates on each
#    platform (Linux, macOS, Windows), configured with the same features: HTTP_ONLY, the
#    CURL_DISABLE_* options listed in config/curl_config.h, no zlib/brotli/zstd/nghttp2/libidn2/
#    libpsl/libssh, threaded resolver, OpenSSL (with CURL_CA_BUNDLE=none, CURL_CA_PATH=none,
#    CURL_CA_FALLBACK=ON) or Schannel. New HAVE_* checks that curl added must be reviewed.

set (CURL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../curl")

# Get CSOURCES from lib/Makefile.inc, the way curl's own CMake does it.
file (READ "${CURL_SOURCE_DIR}/lib/Makefile.inc" _makefile_inc)
string (REGEX REPLACE "\\\\\n" " " _makefile_inc "${_makefile_inc}")                   # join continued lines
string (REGEX REPLACE "#[^\n]*" "" _makefile_inc "${_makefile_inc}")                   # drop comments
string (REGEX REPLACE "([A-Za-z_][A-Za-z0-9_]*)[\t ]*=[\t ]*([^\n]*)" "set(\\1 \\2)" _makefile_inc "${_makefile_inc}")
string (REGEX REPLACE "\\$\\(([A-Za-z_][A-Za-z0-9_]*)\\)" "\${\\1}" _makefile_inc "${_makefile_inc}")
file (WRITE "${CMAKE_CURRENT_BINARY_DIR}/Makefile.inc.cmake" "${_makefile_inc}")
include ("${CMAKE_CURRENT_BINARY_DIR}/Makefile.inc.cmake")
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CURL_SOURCE_DIR}/lib/Makefile.inc")

if (NOT CSOURCES)
    message (FATAL_ERROR "Failed to get the list of curl sources from ${CURL_SOURCE_DIR}/lib/Makefile.inc")
endif ()

list (TRANSFORM CSOURCES PREPEND "${CURL_SOURCE_DIR}/lib/")

add_library (_curl STATIC ${CSOURCES})

target_include_directories (_curl
    PUBLIC "${CURL_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/config"
    PRIVATE "${CURL_SOURCE_DIR}/lib"
)

target_compile_definitions (_curl
    PUBLIC CURL_STATICLIB
    PRIVATE BUILDING_LIBCURL
    PRIVATE HAVE_CONFIG_H
)

if (OS_WINDOWS)
    target_compile_definitions (_curl
        PRIVATE UNICODE  # file and certificate store paths are UTF-8
        PRIVATE _UNICODE
    )
    target_link_libraries (_curl
        PRIVATE ws2_32
        PRIVATE iphlpapi
        PRIVATE bcrypt
        PRIVATE advapi32
        PRIVATE crypt32
        PRIVATE secur32
    )
elseif (OS_LINUX OR OS_DARWIN)
    if (OS_LINUX)
        target_compile_definitions (_curl PRIVATE _GNU_SOURCE)
    else ()
        target_compile_definitions (_curl PRIVATE _DARWIN_C_SOURCE)
        target_link_libraries (_curl
            PRIVATE "-framework SystemConfiguration"
            PRIVATE "-framework CoreFoundation"
            PRIVATE "-framework CoreServices"
        )
    endif ()
    target_link_libraries (_curl
        PRIVATE OpenSSL::SSL
        PRIVATE OpenSSL::Crypto
        PRIVATE Threads::Threads
    )
else ()
    message (FATAL_ERROR "libcurl is not configured for this platform, see contrib/curl-cmake/config/curl_config.h")
endif ()

add_library (ch_contrib::curl ALIAS _curl)
