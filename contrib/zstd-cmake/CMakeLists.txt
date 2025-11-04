set(ZSTD_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)

get_directory_property(SAVED_LINK_LIBS LINK_LIBRARIES)
set_property(DIRECTORY PROPERTY LINK_LIBRARIES "")
add_subdirectory(../zstd/build/cmake ${CMAKE_CURRENT_BINARY_DIR}/contrib/zstd/)
set_property(DIRECTORY PROPERTY LINK_LIBRARIES ${SAVED_LINK_LIBS})
add_library (ch_contrib::zstd ALIAS libzstd_static)
