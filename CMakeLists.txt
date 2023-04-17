#
#  CMakeLists.txt: CMake configuration file for sigutils
#
#  Copyright (C) 2019 Gonzalo José Carracedo Carballal
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as
#  published by the Free Software Foundation, version 3.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this program.  If not, see
#  <http://www.gnu.org/licenses/>
#
#
  
cmake_minimum_required(VERSION 3.12.0)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(FindPkgConfig)
include(CodeAnalysis)
include(GitVersionDetect)
include(RelativeFileMacro)
include(PcFileGenerator)

# Use git version detect to obtain a version
set(SIGUTILS_VERSION_MAJOR ${GITVERSIONDETECT_VERSION_MAJOR})
set(SIGUTILS_VERSION_MINOR ${GITVERSIONDETECT_VERSION_MINOR})
set(SIGUTILS_VERSION_PATCH ${GITVERSIONDETECT_VERSION_PATCH})
set(SIGUTILS_VERSION ${SIGUTILS_VERSION_MAJOR}.${SIGUTILS_VERSION_MINOR}.${SIGUTILS_VERSION_PATCH})

# Set the ABI version manually
set(SIGUTILS_ABI_VERSION   1)

# Define the project
project(
  sigutils
  VERSION ${SIGUTILS_VERSION}
  HOMEPAGE_URL "http://github.org/BatchDrake/sigutils"
  LANGUAGES C)

# Late module imports that depend on project definitions
include(GNUInstallDirs)

# Find requirements
find_package(Threads)

pkg_check_modules(SNDFILE REQUIRED sndfile>=1.0.2)
include_directories(${SNDFILE_INCLUDE_DIRS})
link_directories(${SNDFILE_LIBRARY_DIRS})

pkg_check_modules(FFTW3   REQUIRED fftw3f>=3.0)
include_directories(${FFTW3_INCLUDE_DIRS})
link_directories(${FFTW3_LIBRARY_DIRS})

pkg_check_modules(VOLK             volk>=1.0)
if(VOLK_FOUND)
  include_directories(${VOLK_INCLUDE_DIRS})
  link_directories(${VOLK_LIBRARY_DIRS})
endif()

# Project build options
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
       FORCE)
endif()
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffast-math -s")
endif()

option(SIGUTILS_SINGLE_PRECISSION "Use single precission data types" ON)

if (DEFINED PKGVERSION)
  # If you are building sigutils for your own software distribution, you may want
  # to set PKGVERSION to some descriptive string.
  add_compile_definitions(SIGUTILS_PKGVERSION="${PKGVERSION}")
endif()

# Source location
set(SRCDIR   sigutils)
set(UTILDIR  util)
set(SPECDIR  ${SRCDIR}/specific)

########################## pkg-config description #############################  
set(
  INSTALL_PKGCONFIG_DIR 
  "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  CACHE PATH "Installation directory for pkgconfig (.pc) files")


install(
  FILES "${PROJECT_BINARY_DIR}/sigutils.pc"
  DESTINATION "${INSTALL_PKGCONFIG_DIR}")
  
########################## Sigutils library build #############################
set(SIGUTILS_UTIL_HEADERS ${UTILDIR}/util.h)
set(SIGUTILS_UTIL_SOURCES ${UTILDIR}/util.c)

set(SIGUTILS_COMPAT_SOURCES "")
set(SIGUTILS_COMPAT_HEADERS
   ${UTILDIR}/compat-mman.h
   ${UTILDIR}/compat-time.h
   ${UTILDIR}/compat-stat.h
   ${UTILDIR}/compat-fcntl.h
   ${UTILDIR}/compat-in.h
   ${UTILDIR}/compat-inet.h
   ${UTILDIR}/compat-netdb.h
   ${UTILDIR}/compat-poll.h
   ${UTILDIR}/compat-pwd.h
   ${UTILDIR}/compat-select.h
   ${UTILDIR}/compat-socket.h
   ${UTILDIR}/compat-statvfs.h
   ${UTILDIR}/compat-stdlib.h
   ${UTILDIR}/compat-termios.h
   ${UTILDIR}/compat-unistd.h)

if(WIN32)
  set(SIGUTILS_COMPAT_HEADERS
    ${SIGUTILS_COMPAT_HEADERS}
    ${UTILDIR}/win32-mman.h
    ${UTILDIR}/win32-time.h
    ${UTILDIR}/win32-stat.h
    ${UTILDIR}/win32-fcntl.h
    ${UTILDIR}/win32-in.h
    ${UTILDIR}/win32-inet.h
    ${UTILDIR}/win32-netdb.h
    ${UTILDIR}/win32-poll.h
    ${UTILDIR}/win32-pwd.h
    ${UTILDIR}/win32-socket.h
    ${UTILDIR}/win32-statvfs.h
    ${UTILDIR}/win32-stdlib.h
    ${UTILDIR}/win32-termios.h
    ${UTILDIR}/win32-unistd.h)

  set(SIGUTILS_COMPAT_SOURCES
    ${SIGUTILS_COMPAT_SOURCES}
    ${UTILDIR}/win32-fcntl.c
    ${UTILDIR}/win32-mman.c
    ${UTILDIR}/win32-time.c
    ${UTILDIR}/win32-poll.c
    ${UTILDIR}/win32-pwd.c
    ${UTILDIR}/win32-statvfs.c
    ${UTILDIR}/win32-stdlib.c
    ${UTILDIR}/win32-termios.c
    ${UTILDIR}/win32-unistd.c)
endif()

set(SIGUTILS_SPECIFIC_HEADERS
    ${SPECDIR}/apt.h)
set(SIGUTILS_SPECIFIC_SOURCES
    ${SPECDIR}/apt.c)

set(SIGUTILS_LIB_HEADERS
    ${SRCDIR}/agc.h
    ${SRCDIR}/block.h
    ${SRCDIR}/clock.h
    ${SRCDIR}/coef.h
    ${SRCDIR}/decider.h
    ${SRCDIR}/defs.h
    ${SRCDIR}/detect.h
    ${SRCDIR}/equalizer.h
    ${SRCDIR}/iir.h
    ${SRCDIR}/lfsr.h
    ${SRCDIR}/log.h
    ${SRCDIR}/matfile.h
    ${SRCDIR}/ncqo.h
    ${SRCDIR}/pll.h
    ${SRCDIR}/property.h
    ${SRCDIR}/sampling.h
    ${SRCDIR}/sigutils.h
    ${SRCDIR}/smoothpsd.h
    ${SRCDIR}/softtune.h
    ${SRCDIR}/specttuner.h
    ${SRCDIR}/taps.h
    ${SRCDIR}/tvproc.h
    ${SRCDIR}/types.h
    ${SRCDIR}/version.h)
    
set(SIGUTILS_LIB_SOURCES 
    ${SRCDIR}/agc.c
    ${SRCDIR}/block.c
    ${SRCDIR}/clock.c
    ${SRCDIR}/coef.c
    ${SRCDIR}/detect.c
    ${SRCDIR}/equalizer.c
    ${SRCDIR}/iir.c
    ${SRCDIR}/lfsr.c
    ${SRCDIR}/lib.c
    ${SRCDIR}/log.c
    ${SRCDIR}/matfile.c
    ${SRCDIR}/ncqo.c
    ${SRCDIR}/pll.c
    ${SRCDIR}/property.c
    ${SRCDIR}/smoothpsd.c
    ${SRCDIR}/softtune.c
    ${SRCDIR}/specttuner.c
    ${SRCDIR}/taps.c
    ${SRCDIR}/tvproc.c
    ${SRCDIR}/version.c)

link_directories(${PROJECT_BINARY_DIR})
	
add_library(
  sigutils SHARED
  ${SIGUTILS_COMPAT_SOURCES}
  ${SIGUTILS_COMPAT_HEADERS}
  ${SIGUTILS_UTIL_SOURCES}
  ${SIGUTILS_UTIL_HEADERS}
  ${SIGUTILS_LIB_SOURCES}
  ${SIGUTILS_LIB_HEADERS}
  ${SIGUTILS_SPECIFIC_SOURCES})

if(SIGUTILS_SINGLE_PRECISSION)
  target_compile_definitions(sigutils PUBLIC _SU_SINGLE_PRECISION)
endif()

if(VOLK_FOUND)
target_compile_definitions(sigutils PUBLIC HAVE_VOLK)
endif()

target_add_relative_file_macro(sigutils)

set_property(TARGET sigutils PROPERTY VERSION   ${SIGUTILS_VERSION})
set_property(TARGET sigutils PROPERTY SOVERSION ${SIGUTILS_ABI_VERSION})

# Private header directories
target_include_directories(sigutils PRIVATE . util ${SRCDIR})

# Required dependencies
target_link_libraries(sigutils ${SNDFILE_LIBRARIES})
target_link_libraries(sigutils ${FFTW3_LIBRARIES})
target_link_libraries(sigutils ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sigutils m)

if(WIN32)
  # In Windows systems, winsock is required
  target_link_libraries(sigutils ws2_32)
endif()

# Optional dependencies
if(VOLK_FOUND)
  target_link_libraries(sigutils ${VOLK_LIBRARIES})
endif()

install(
  FILES ${SIGUTILS_LIB_HEADERS} 
  DESTINATION include/sigutils/sigutils)

install(
  FILES ${SIGUTILS_SPECIFIC_HEADERS} 
  DESTINATION include/sigutils/sigutils/specific)

install(
  FILES ${SIGUTILS_UTIL_HEADERS} ${SIGUTILS_COMPAT_HEADERS}
  DESTINATION include/sigutils/util)

install(TARGETS sigutils DESTINATION ${CMAKE_INSTALL_LIBDIR})

target_pc_file_generate(sigutils "Digital signal processing utility library")
