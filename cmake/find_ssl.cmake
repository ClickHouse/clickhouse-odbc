if (NOT MSVC)
    option (USE_INTERNAL_SSL_LIBRARY "Set to FALSE to use system *ssl library instead of bundled" ${NOT_UNBUNDLED})
endif ()

set (OPENSSL_USE_STATIC_LIBS ${USE_STATIC_LIBRARIES})

set (OPENSSL_FOUND)

if (NOT USE_INTERNAL_SSL_LIBRARY)
    if (APPLE)
        set (OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
        # https://rt.openssl.org/Ticket/Display.html?user=guest&pass=guest&id=2232
        if (USE_STATIC_LIBRARIES)
            message(WARNING "Disable USE_STATIC_LIBRARIES if you have linking problems with OpenSSL on MacOS")
        endif ()
    endif ()

    set(_save ${CMAKE_FIND_LIBRARY_SUFFIXES})

    # If you got error
    # /usr/bin/ld: /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/libssl.a(s23_srvr.o): relocation R_X86_64_PC32 against symbol `ssl23_get_client_hello' can not be used when making a shared object; recompile with -fPIC
    # ues this option:
    if (OPENSSL_USE_SHARED_LIBS)
        list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
    endif ()

    find_package (OpenSSL)

    if (NOT OPENSSL_FOUND)
        # Try to find manually.
        set (OPENSSL_INCLUDE_PATHS "/usr/local/opt/openssl/include")
        set (OPENSSL_PATHS "/usr/local/opt/openssl/lib")
        find_path (OPENSSL_INCLUDE_DIR NAMES openssl/ssl.h PATHS ${OPENSSL_INCLUDE_PATHS})
        find_library (OPENSSL_SSL_LIBRARY ssl PATHS ${OPENSSL_PATHS})
        find_library (OPENSSL_CRYPTO_LIBRARY crypto PATHS ${OPENSSL_PATHS})
        if (OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_INCLUDE_DIR)
            set (OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
            set (OPENSSL_FOUND 1 CACHE INTERNAL "")
        endif ()
    endif ()

    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_save})

endif ()

if (NOT OPENSSL_FOUND)
    set (USE_INTERNAL_SSL_LIBRARY 1)
    set (OPENSSL_ROOT_DIR "${PROJECT_SOURCE_DIR}/contrib/ssl" CACHE INTERNAL "")
    set (OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE INTERNAL "")
    if (NOT USE_STATIC_LIBRARIES AND TARGET crypto-shared AND TARGET ssl-shared)
        set (OPENSSL_CRYPTO_LIBRARY crypto-shared CACHE INTERNAL "")
        set (OPENSSL_SSL_LIBRARY ssl-shared CACHE INTERNAL "")
    else ()
        set (OPENSSL_CRYPTO_LIBRARY crypto CACHE INTERNAL "")
        set (OPENSSL_SSL_LIBRARY ssl CACHE INTERNAL "")
    endif ()
    set (OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    set (OPENSSL_FOUND 1 CACHE INTERNAL "")
endif ()

#TODO: to use with new poco
#if (0)
# part from /usr/share/cmake-*/Modules/FindOpenSSL.cmake, with removed all "EXISTS "
if(OPENSSL_FOUND)
  if(NOT TARGET OpenSSL::Crypto AND
      (EXISTS OPENSSL_CRYPTO_LIBRARY OR
        EXISTS LIB_EAY_LIBRARY_DEBUG OR
        EXISTS LIB_EAY_LIBRARY_RELEASE)
      )
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    if(OPENSSL_CRYPTO_LIBRARY)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")
    endif()
    if(LIB_EAY_LIBRARY_RELEASE)
      set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${LIB_EAY_LIBRARY_RELEASE}")
    endif()
    if(LIB_EAY_LIBRARY_DEBUG)
      set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${LIB_EAY_LIBRARY_DEBUG}")
    endif()
  endif()
  if(NOT TARGET OpenSSL::SSL AND
      (OPENSSL_SSL_LIBRARY OR
        SSL_EAY_LIBRARY_DEBUG OR
        SSL_EAY_LIBRARY_RELEASE)
      )
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    if(OPENSSL_SSL_LIBRARY)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")
    endif()
    if(SSL_EAY_LIBRARY_RELEASE)
      set_property(TARGET OpenSSL::SSL APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${SSL_EAY_LIBRARY_RELEASE}")
    endif()
    if(SSL_EAY_LIBRARY_DEBUG)
      set_property(TARGET OpenSSL::SSL APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${SSL_EAY_LIBRARY_DEBUG}")
    endif()
    if(TARGET OpenSSL::Crypto)
      set_target_properties(OpenSSL::SSL PROPERTIES
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
    endif()
  endif()
endif()
#endif()

message (STATUS "Using ssl=${OPENSSL_FOUND}: ${OPENSSL_INCLUDE_DIR} : ${OPENSSL_LIBRARIES}")
