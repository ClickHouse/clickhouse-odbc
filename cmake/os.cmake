
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set (OS_LINUX 1)
    add_definitions(-D OS_LINUX)
elseif (CMAKE_SYSTEM_NAME MATCHES "Android")
    # This is a toy configuration and not in CI, so expect it to be broken.
    # Use cmake flags such as: -DCMAKE_TOOLCHAIN_FILE=~/ch2/android-ndk-r21d/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=28
    set (OS_ANDROID 1)
    add_definitions(-D OS_ANDROID)
elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set (OS_FREEBSD 1)
    add_definitions(-D OS_FREEBSD)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set (OS_DARWIN 1)
    add_definitions(-D OS_DARWIN)
    # For MAP_ANON/MAP_ANONYMOUS
    add_definitions(-D _DARWIN_C_SOURCE)
elseif (CMAKE_SYSTEM_NAME MATCHES "SunOS")
    set (OS_SUNOS 1)
    add_definitions(-D OS_SUNOS)
else ()
    message (FATAL_ERROR "Platform '${CMAKE_SYSTEM_NAME}' is not supported")
endif ()