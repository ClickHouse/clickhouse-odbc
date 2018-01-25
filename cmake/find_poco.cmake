option (USE_INTERNAL_POCO_LIBRARY "Set to FALSE to use system poco library instead of bundled" 1)

if (NOT USE_INTERNAL_POCO_LIBRARY)

    if (WIN32 OR MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
    elseif (UNIX)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif ()

    find_package (Poco COMPONENTS Net)
endif ()

if (Poco_INCLUDE_DIRS AND Poco_Foundation_LIBRARY)
    include_directories (${Poco_INCLUDE_DIRS})
else ()
    set (POCO_STATIC 1 CACHE BOOL "")

    set (ENABLE_CPPUNIT 0 CACHE BOOL "")
    set (ENABLE_XML 0 CACHE BOOL "")
    set (ENABLE_JSON 0 CACHE BOOL "")
    set (ENABLE_MONGODB 0 CACHE BOOL "")
    if (NOT USE_SSL)
        set (ENABLE_NETSSL 0 CACHE BOOL "")
        set (ENABLE_CRYPTO 0 CACHE BOOL "")
        set (ENABLE_UTIL 0 CACHE BOOL "")
    endif ()
    set (ENABLE_DATA 0 CACHE BOOL "")
    set (ENABLE_ZIP 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER_FILE2PAGE 0 CACHE BOOL "")
    set (ENABLE_REDIS 0 CACHE BOOL "")

    set (USE_INTERNAL_POCO_LIBRARY 1)

    if (USE_SSL)
        set (Poco_NetSSL_FOUND 1)
        set (Poco_NetSSL_LIBRARY PocoNetSSL)
        set (Poco_Crypto_LIBRARY PocoCrypto)
    endif ()

    set (Poco_Foundation_LIBRARY PocoFoundation)
    set (Poco_Util_LIBRARY PocoUtil)
    set (Poco_Net_LIBRARY PocoNet)
endif ()

message(STATUS "Using Poco: ${Poco_INCLUDE_DIRS} : ${Poco_Foundation_LIBRARY},${Poco_Util_LIBRARY},${Poco_Net_LIBRARY}")
