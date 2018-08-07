option (USE_INTERNAL_POCO_LIBRARY "Set to FALSE to use system poco library instead of bundled" ${NOT_UNBUNDLED})

if (NOT USE_INTERNAL_POCO_LIBRARY)
    if (WIN32 OR MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
    #elseif (UNIX)
    #   set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif ()

    set (POCO_COMPONENTS Net)
    if (NOT DEFINED ENABLE_POCO_NETSSL OR ENABLE_POCO_NETSSL)
        list (APPEND POCO_COMPONENTS Crypto NetSSL)
    endif ()

    find_package (Poco COMPONENTS ${POCO_COMPONENTS})
endif ()

if (Poco_INCLUDE_DIRS AND Poco_Foundation_LIBRARY)
    #include_directories (${Poco_INCLUDE_DIRS})
else ()
    set (POCO_STATIC 1 CACHE BOOL "")

    set (ENABLE_CPPUNIT 0 CACHE BOOL "")
    set (ENABLE_XML 0 CACHE BOOL "")
    set (ENABLE_JSON 0 CACHE BOOL "")
    set (ENABLE_MONGODB 0 CACHE BOOL "")
    set (ENABLE_DATA 0 CACHE BOOL "")
    set (ENABLE_ZIP 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER_FILE2PAGE 0 CACHE BOOL "")
    set (ENABLE_REDIS 0 CACHE BOOL "")
    set (POCO_ENABLE_CPPUNIT 0 CACHE BOOL "")
    set (POCO_ENABLE_XML 0 CACHE BOOL "")
    set (POCO_ENABLE_JSON 0 CACHE BOOL "")
    set (POCO_ENABLE_MONGODB 0 CACHE BOOL "")
    set (POCO_ENABLE_DATA 0 CACHE BOOL "")
    set (POCO_ENABLE_SQL 0 CACHE BOOL "")
    set (POCO_ENABLE_ZIP 0 CACHE BOOL "")
    set (POCO_ENABLE_PAGECOMPILER 0 CACHE BOOL "")
    set (POCO_ENABLE_PAGECOMPILER_FILE2PAGE 0 CACHE BOOL "")
    set (POCO_ENABLE_REDIS 0 CACHE BOOL "")
    if (NOT USE_SSL)
        set (POCO_ENABLE_NETSSL 0 CACHE BOOL "")
        set (POCO_ENABLE_CRYPTO 0 CACHE BOOL "")
        set (POCO_ENABLE_UTIL 0 CACHE BOOL "")
        set (ENABLE_NETSSL 0 CACHE BOOL "")
        set (ENABLE_CRYPTO 0 CACHE BOOL "")
        set (ENABLE_UTIL 0 CACHE BOOL "")
    endif ()

    set (USE_INTERNAL_POCO_LIBRARY 1)

    if (USE_SSL)
        set (Poco_NetSSL_FOUND 1)
        set (Poco_NetSSL_LIBRARY PocoNetSSL)
        set (Poco_Crypto_LIBRARY PocoCrypto)
        list (APPEND Poco_INCLUDE_DIRS
            "${clickhouse-odbc_SOURCE_DIR}/contrib/poco/NetSSL_OpenSSL/include"
            "${clickhouse-odbc_SOURCE_DIR}/contrib/poco/Crypto/include"
        )

    endif ()

    set (Poco_Foundation_LIBRARY PocoFoundation)
    set (Poco_Util_LIBRARY PocoUtil)
    set (Poco_Net_LIBRARY PocoNet)
endif ()

message(STATUS "Using Poco: ${Poco_INCLUDE_DIRS} : ${Poco_Foundation_LIBRARY},${Poco_Util_LIBRARY},${Poco_Net_LIBRARY},${Poco_NetSSL_LIBRARY},${Poco_Crypto_LIBRARY},${Poco_XML_LIBRARY},${Poco_Data_LIBRARY},${Poco_DataODBC_LIBRARY},${Poco_SQL_LIBRARY},${Poco_SQLODBC_LIBRARY},${Poco_MongoDB_LIBRARY}; MongoDB=${USE_POCO_MONGODB}, DataODBC=${USE_POCO_DATAODBC}, NetSSL=${USE_POCO_NETSSL}")
