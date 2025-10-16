# Copyright Siemens AG, 2014
# Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
# and Contributors.
#
# SPDX-License-Identifier:	BSL-1.0
#
# Collection of common functionality for Poco CMake

# Find the Microsoft mc.exe message compiler
#
# CMAKE_MC_COMPILER - where to find mc.exe
if(WIN32)
	# cmake has CMAKE_RC_COMPILER, but no message compiler
	if("${CMAKE_GENERATOR}" MATCHES "Visual Studio" OR MINGW)
		# this path is only present for 2008+, but we currently require PATH to
		# be set up anyway
		get_filename_component(sdk_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]" REALPATH)
		get_filename_component(kit_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot]" REALPATH)
		get_filename_component(kit81_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot81]" REALPATH)
		get_filename_component(kit10_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" REALPATH)
		get_filename_component(kit10wow_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" REALPATH)
		file(GLOB kit10_list ${kit10_dir}/bin/10.* ${kit10wow_dir}/bin/10.*)
		if(X64)
			set(sdk_bindir "${sdk_dir}/bin/x64")
			set(kit_bindir "${kit_dir}/bin/x64")
			set(kit81_bindir "${kit81_dir}/bin/x64")
			foreach(tmp_elem ${kit10_list})
				if(IS_DIRECTORY ${tmp_elem})
			list(APPEND kit10_bindir "${tmp_elem}/x64")
				endif()
			endforeach()
		else(X64)
			set(sdk_bindir "${sdk_dir}/bin")
			set(kit_bindir "${kit_dir}/bin/x86")
			set(kit81_bindir "${kit81_dir}/bin/x86")
			foreach(tmp_elem ${kit10_list})
				if(IS_DIRECTORY ${tmp_elem})
			list(APPEND kit10_bindir "${tmp_elem}/x86")
				endif()
			endforeach()
		endif(X64)
	endif()
	find_program(CMAKE_MC_COMPILER mc.exe HINTS "${sdk_bindir}" "${kit_bindir}" "${kit81_bindir}" ${kit10_bindir}
		DOC "path to message compiler")
	if(NOT CMAKE_MC_COMPILER)
		message(FATAL_ERROR "message compiler not found: required to build")
	endif(NOT CMAKE_MC_COMPILER)
	message(STATUS "Found message compiler: ${CMAKE_MC_COMPILER}")
	mark_as_advanced(CMAKE_MC_COMPILER)
endif(WIN32)

#===============================================================================
#  Macros for Source file management
#
#  POCO_SOURCES_PLAT - Adds a list of files to the sources of a components
#	Usage: POCO_SOURCES_PLAT( out name platform sources)
#	  INPUT:
#		   out			 the variable the sources are added to
#		   name:		   the name of the components
#		   platform:	   the platform this sources are for (ON = All, OFF = None, WIN32, UNIX ...)
#		   sources:		a list of files to add to ${out}
#	Example: POCO_SOURCES_PLAT( SRCS Foundation ON src/Foundation.cpp )
#
#  POCO_SOURCES - Like POCO_SOURCES_PLAT with platform = ON (Built on all platforms)
#	Usage: POCO_SOURCES( out name sources)
#	Example: POCO_SOURCES( SRCS Foundation src/Foundation.cpp)
#
#  POCO_SOURCES_AUTO - Like POCO_SOURCES but the name is read from the file header // Package: X
#	Usage: POCO_SOURCES_AUTO( out sources)
#	Example: POCO_SOURCES_AUTO( SRCS src/Foundation.cpp)
#
#  POCO_SOURCES_AUTO_PLAT - Like POCO_SOURCES_PLAT but the name is read from the file header // Package: X
#	Usage: POCO_SOURCES_AUTO_PLAT(out platform sources)
#	Example: POCO_SOURCES_AUTO_PLAT( SRCS WIN32 src/Foundation.cpp)
#
#
#  POCO_HEADERS - Adds a list of files to the headers of a components
#	Usage: POCO_HEADERS( out name headers)
#	  INPUT:
#		   out			 the variable the headers are added to
#		   name:		   the name of the components
#		   headers:		a list of files to add to HDRSt
#	Example: POCO_HEADERS( HDRS Foundation include/Poco/Foundation.h )
#
#  POCO_HEADERS_AUTO - Like POCO_HEADERS but the name is read from the file header // Package: X
#	Usage: POCO_HEADERS_AUTO( out headers)
#	Example: POCO_HEADERS_AUTO( HDRS src/Foundation.cpp)
#
#
#  POCO_MESSAGES - Adds a list of files to the messages of a components
#				  and adds the generated headers to the header list of the component.
#				  On platforms other then Windows this does nothing
#	Usage: POCO_MESSAGES( out name messages)
#	  INPUT:
#		   out			 the variable the message and the resulting headers are added to
#		   name:		   the name of the components
#		   messages:	   a list of files to add to MSGS
#	Example: POCO_MESSAGES( HDRS Foundation include/Poco/Foundation.mc )
#

macro(POCO_SOURCES_PLAT out name platform)
	source_group("${name}\\Source Files" FILES ${ARGN})
	list(APPEND ${out} ${ARGN})
	if(NOT(${platform}))
		set_source_files_properties(${ARGN} PROPERTIES HEADER_FILE_ONLY TRUE)
	endif()
endmacro()

macro(POCO_SOURCES out name)
	POCO_SOURCES_PLAT( ${out} ${name} ON ${ARGN})
endmacro()

macro(POCO_SOURCES_AUTO out)
	POCO_SOURCES_AUTO_PLAT( ${out} ON ${ARGN})
endmacro()

macro(POCO_SOURCES_AUTO_PLAT out platform)
	foreach(f ${ARGN})
		get_filename_component(fname ${f} NAME)

		# Read the package name from the source file
		file(STRINGS ${f} package REGEX "// Package: (.*)")
		if(package)
			string(REGEX REPLACE ".*: (.*)" "\\1" name ${package})

			# Files of the Form X_UNIX.cpp are treated as headers
			if(${fname} MATCHES ".*_.*\\..*")
				#message(STATUS "Platform: ${name} ${f} ${platform}")
				POCO_SOURCES_PLAT( ${out} ${name} OFF ${f})
			else()
				#message(STATUS "Source: ${name} ${f} ${platform}")
				POCO_SOURCES_PLAT( ${out} ${name} ${platform} ${f})
			endif()
		else()
			#message(STATUS "Source: Unknown ${f} ${platform}")
			POCO_SOURCES_PLAT( ${out} Unknown ${platform} ${f})
		endif()
	endforeach()
endmacro()

macro(POCO_HEADERS_AUTO out)
	foreach(f ${ARGN})
		get_filename_component(fname ${f} NAME)

		# Read the package name from the source file
		file(STRINGS ${f} package REGEX "// Package: (.*)")
		if(package)
			string(REGEX REPLACE ".*: (.*)" "\\1" name ${package})
			#message(STATUS "Header: ${name} ${f}")
			POCO_HEADERS( ${out} ${name} ${f})
		else()
			#message(STATUS "Header: Unknown ${f}")
			POCO_HEADERS( ${out} Unknown ${f})
		endif()
	endforeach()
endmacro()

macro(POCO_HEADERS out name)
	set_source_files_properties(${ARGN} PROPERTIES HEADER_FILE_ONLY TRUE)
	source_group("${name}\\Header Files" FILES ${ARGN})
	list(APPEND ${out} ${ARGN})
endmacro()

macro(POCO_MESSAGES out name)
	if(WIN32)
		foreach(msg ${ARGN})
			get_filename_component(msg_name ${msg} NAME)
			get_filename_component(msg_path ${msg} ABSOLUTE)
			string(REPLACE ".mc" ".h" hdr ${msg_name})
			set_source_files_properties(${hdr} PROPERTIES GENERATED TRUE)
			string(REPLACE ".mc" ".rc" rc ${msg_name})
			set_source_files_properties(${rc} PROPERTIES GENERATED TRUE)
			add_custom_command(
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${hdr} ${CMAKE_CURRENT_BINARY_DIR}/${rc}
				DEPENDS ${msg}
				COMMAND ${CMAKE_MC_COMPILER}
				ARGS
					-h ${CMAKE_CURRENT_BINARY_DIR}
					-r ${CMAKE_CURRENT_BINARY_DIR}
					${msg_path}
				VERBATIM # recommended: p260
			)

			# Add the generated file to the include directory
			include_directories(${CMAKE_CURRENT_BINARY_DIR})

			# Add the generated headers to POCO_HEADERS of the component
			POCO_HEADERS( ${out} ${name} ${CMAKE_CURRENT_BINARY_DIR}/${hdr})

			# Add the generated .rc 
			if(BUILD_SHARED_LIBS)
				source_group("${name}\\Resource Files" FILES ${CMAKE_CURRENT_BINARY_DIR}/${rc})
				list(APPEND ${out} ${CMAKE_CURRENT_BINARY_DIR}/${rc})
			endif()
		endforeach()

		set_source_files_properties(${ARGN} PROPERTIES HEADER_FILE_ONLY TRUE)
		source_group("${name}\\Message Files" FILES ${ARGN})
		list(APPEND ${out} ${ARGN})

	endif(WIN32)
endmacro()

