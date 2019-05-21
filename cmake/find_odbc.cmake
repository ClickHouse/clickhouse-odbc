# This file copied from contrib/poco/cmake/FindODBC.cmake to allow build without submodules

#
# Find the ODBC driver manager includes and library.
#
# ODBC is an open standard for connecting to different databases in a
# semi-vendor-independent fashion.  First you install the ODBC driver
# manager.  Then you need a driver for each separate database you want
# to connect to (unless a generic one works).  VTK includes neither
# the driver manager nor the vendor-specific drivers: you have to find
# those yourself.
#
# This module defines
# ODBC_INCLUDE_DIRECTORIES, where to find sql.h
# ODBC_LIBRARIES, the libraries to link against to use ODBC
# ODBC_FOUND.  If false, you cannot build anything that requires MySQL.

find_path(ODBC_INCLUDE_DIRECTORIES
	NAMES sql.h
	PATHS
    /usr/local/include/libiodbc
	/usr/local/include
	/usr/local/include/odbc
	/usr/local/include/iodbc
	/usr/local/odbc/include
	/usr/local/iodbc/include
	/usr/include
	/usr/include/odbc
	/usr/include/iodbc
	"C:/Program Files/ODBC/include"
	"C:/Program Files/Microsoft SDKs/Windows/v7.0/include"
	"C:/Program Files/Microsoft SDKs/Windows/v6.0a/include"
	"C:/ODBC/include"
	DOC "Specify the directory containing sql.h."
)

set(ODBC_LIBRARIES_PATHS
	/usr/local/lib
	/usr/local/lib/odbc
	/usr/local/lib/iodbc
	/usr/local/odbc/lib
	/usr/local/iodbc/lib
	/usr/lib
	/usr/lib/odbc
	/usr/lib/iodbc
	"C:/Program Files/ODBC/lib"
	"C:/ODBC/lib/debug"
	"C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib"
)

# Macos cheat: libiodbc is always masked as odbc
if((APPLE AND NOT FIND_UNIXODBC_FIRST) OR FIND_IODBC_FIRST)
    find_library(ODBC_LIBRARIES NAMES iodbc PATHS ${ODBC_LIBRARIES_PATHS})
endif()

if(NOT ODBC_LIBRARIES)
    find_library(ODBC_LIBRARIES NAMES odbc odbc32 PATHS ${ODBC_LIBRARIES_PATHS} DOC "Specify the ODBC driver manager library here.")
endif()

# Macos cheat: libiodbc is always installed, but sometimes we want to build with unixodbc, so searching iodbc after unixodbc: 
if(NOT ODBC_LIBRARIES)
    find_library(ODBC_LIBRARIES NAMES iodbc PATHS ${ODBC_LIBRARIES_PATHS})
endif()

if(NOT WIN32)
    if(ODBC_LIBRARIES MATCHES "iodbc")
        set(ODBC_IODBC 1)
        set(ODBC_UNIXODBC 0)
    else() # TODO maybe better check
        set(ODBC_IODBC 0)
        set(ODBC_UNIXODBC 1)
    endif()
endif()


if(NOT WIN32)
    if (ODBC_UNIXODBC)
        find_library(ODBCINST_LIBRARIES NAMES odbcinst PATHS ${ODBC_LIBRARIES_PATHS})
    endif()
    if(ODBC_IODBC)
       find_library(ODBCINST_LIBRARIES NAMES iodbcinst PATHS ${ODBC_LIBRARIES_PATHS})
    endif()
    list(APPEND ODBC_LIBRARIES ${ODBCINST_LIBRARIES})
    list(APPEND ODBC_LIBRARIES ${LTDL_LIBRARY})
endif()

# MinGW find usually fails
if(MINGW)
	set(ODBC_INCLUDE_DIRECTORIES ".")
	set(ODBC_LIBRARIES odbc32)
endif()
	
include(FindPackageHandleStandardArgs DEFAULT_MSG)
find_package_handle_standard_args(ODBC
	DEFAULT_MSG
	ODBC_INCLUDE_DIRECTORIES
	ODBC_LIBRARIES
	)

mark_as_advanced(ODBC_FOUND ODBC_LIBRARIES ODBC_INCLUDE_DIRECTORIES)

message(STATUS "Using odbc: ${ODBC_INCLUDE_DIRECTORIES} : ${ODBC_LIBRARIES}  ODBC_IODBC=${ODBC_IODBC} ODBC_UNIXODBC=${ODBC_UNIXODBC}")
