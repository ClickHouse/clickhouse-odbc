include(cmake/PocoMacros.cmake)

# Only Foundation is used. HTTP/TLS is provided by libcurl (see contrib/curl-cmake).
add_subdirectory(Foundation)
