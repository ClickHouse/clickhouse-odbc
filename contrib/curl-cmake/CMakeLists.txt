# libcurl, built from the unmodified upstream sources (contrib/curl) with its own CMake.
#
# Only HTTP(S) is enabled and all optional third-party dependencies are disabled. TLS is
# provided by the bundled OpenSSL on Linux/macOS and by the native Schannel on Windows.
# Content decoding (zstd) is done by the driver itself, see the motivation at
# HttpResponse::Zstd in driver/http_client.cpp.

set (CURL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../curl")

# Build options. These are directory-scoped normal variables where possible, cache entries
# (FORCE) where curl reads them through option()/set(CACHE).
set (BUILD_SHARED_LIBS OFF)
set (BUILD_TESTING OFF)
set (BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set (BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set (BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set (BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set (ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set (CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set (CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
set (CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set (PICKY_COMPILER OFF CACHE BOOL "" FORCE)
set (CURL_HIDDEN_SYMBOLS OFF CACHE BOOL "" FORCE) # do not export curl_* from the driver library

# Features: HTTP/1.1 only, no auth schemes besides Basic (the driver sets the header itself).
set (HTTP_ONLY ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_ALTSVC ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_AWS ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_COOKIES ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_DOH ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_HSTS ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_KERBEROS_AUTH ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_NEGOTIATE_AUTH ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_NETRC ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_NTLM ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_SRP ON CACHE BOOL "" FORCE)
set (CURL_DISABLE_WEBSOCKETS ON CACHE BOOL "" FORCE)
set (ENABLE_UNIX_SOCKETS OFF CACHE BOOL "" FORCE)
set (ENABLE_THREADED_RESOLVER ON CACHE BOOL "" FORCE)
set (ENABLE_ARES OFF CACHE BOOL "" FORCE)

# No optional dependencies.
set (CURL_ZLIB OFF CACHE STRING "" FORCE)
set (CURL_BROTLI OFF CACHE STRING "" FORCE)
set (CURL_ZSTD OFF CACHE STRING "" FORCE)
set (USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set (USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set (CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set (CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set (CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
set (CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)

if (OS_WINDOWS)
    set (CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set (CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set (CURL_STATIC_CRT ON CACHE BOOL "" FORCE)
    set (ENABLE_UNICODE ON CACHE BOOL "" FORCE) # file and certificate store paths are UTF-8
else ()
    set (CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)

    # Do not bake the CA bundle location of the build machine into the binary: use the
    # default verify paths of the bundled OpenSSL (OPENSSLDIR), like Poco did.
    set (CURL_CA_BUNDLE "none" CACHE STRING "" FORCE)
    set (CURL_CA_PATH "none" CACHE STRING "" FORCE)
    set (CURL_CA_FALLBACK ON CACHE BOOL "" FORCE)

    # Make curl's find_package(OpenSSL) resolve to the bundled OpenSSL targets
    # (OpenSSL::SSL and OpenSSL::Crypto are defined by contrib/openssl-cmake).
    get_target_property (_openssl_include_dirs crypto INTERFACE_INCLUDE_DIRECTORIES)
    list (GET _openssl_include_dirs 0 _openssl_include_dir)
    set (OPENSSL_INCLUDE_DIR "${_openssl_include_dir}" CACHE PATH "" FORCE)
    set (OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto CACHE STRING "" FORCE)
    set (OPENSSL_SSL_LIBRARY OpenSSL::SSL CACHE STRING "" FORCE)
    set (OPENSSL_USE_STATIC_LIBS ON)

    # try_compile() cannot link targets of this build, so the results of curl's OpenSSL
    # feature checks are provided for the bundled OpenSSL (3.x) here. Re-validate these
    # when updating contrib/curl or contrib/openssl.
    set (HAVE_BORINGSSL 0 CACHE INTERNAL "")
    set (HAVE_AWSLC 0 CACHE INTERNAL "")
    set (HAVE_LIBRESSL 0 CACHE INTERNAL "")
    set (HAVE_DES_ECB_ENCRYPT 1 CACHE INTERNAL "")
    set (HAVE_SSL_SET0_WBIO 1 CACHE INTERNAL "")
endif ()

# Like the other contribs: do not propagate the driver's global link options into curl.
get_directory_property (SAVED_LINK_LIBS LINK_LIBRARIES)
set_property (DIRECTORY PROPERTY LINK_LIBRARIES "")
add_subdirectory ("${CURL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/curl" EXCLUDE_FROM_ALL)
set_property (DIRECTORY PROPERTY LINK_LIBRARIES ${SAVED_LINK_LIBS})

add_library (ch_contrib::curl ALIAS libcurl_static)
