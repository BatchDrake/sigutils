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

set(SIGUTILS_VERSION_MAJOR 0)
set(SIGUTILS_VERSION_MINOR 3)
set(SIGUTILS_VERSION_PATCH 0)

set(SIGUTILS_ABI_VERSION   1)

set(
  SIGUTILS_VERSION
  ${SIGUTILS_VERSION_MAJOR}.${SIGUTILS_VERSION_MINOR}.${SIGUTILS_VERSION_PATCH})

project(
  sigutils
  VERSION ${SIGUTILS_VERSION}
  LANGUAGES C)

include(FindPkgConfig)

# Find requirements
find_package(Threads)
pkg_check_modules(SNDFILE REQUIRED sndfile>=1.0.2)
pkg_check_modules(FFTW3   REQUIRED fftw3f>=3.0)
pkg_check_modules(VOLK              volk>=1.0)

# Find cppcheck (if available)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
include(CodeAnalysis)

# Source location
set(SRCDIR   sigutils)
set(UTILDIR  util)
set(SPECDIR  ${SRCDIR}/specific)

# Compiler configuration
add_compile_definitions(_SU_SINGLE_PRECISION)
if(VOLK_FOUND)
  add_compile_definitions(HAVE_VOLK)
  link_directories(${VOLK_LIBRARY_DIRS})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfo
MinSizeRel."
       FORCE )
endif()


# The following hack exposes __FILENAME__ to source files as the relative
# path of each source file.
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

#
# If you are building sigutils for your own software distribution, you may want
# to set PKGVERSION to some descriptive string.
#

if (DEFINED PKGVERSION)
  set(
    CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -DSIGUTILS_PKGVERSION='\"${PKGVERSION}\"'")
endif()
  
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -ffast-math -s -DNDEBUG")

########################## pkg-config description #############################
set(SIGUTILS_PC_FILE_PATH "${PROJECT_BINARY_DIR}/sigutils.pc")
  
set(
  INSTALL_PKGCONFIG_DIR 
  "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  CACHE PATH "Installation directory for pkgconfig (.pc) files")
set(SU_PC_CFLAGS "${SIGUTILS_CONFIG_CFLAGS}")
string(REPLACE ";" " " SNDFILE_SPC_LDFLAGS "${SNDFILE_LDFLAGS}")
string(REPLACE ";" " " FFTW3_SPC_LDFLAGS "${FFTW3_LDFLAGS}")
string(REPLACE ";" " " VOLK_SPC_LDFLAGS "${VOLK_LDFLAGS}")

set(SU_PC_LIBRARIES "-lm")
configure_file(sigutils.pc.in "${SIGUTILS_PC_FILE_PATH}" @ONLY)

install(
  FILES "${SIGUTILS_PC_FILE_PATH}"
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

link_directories(${PROJECT_BINARY_DIR} ${SNDFILE_LIBRARY_DIRS} ${FFTW3_LIBRARY_DIRS})
	
add_library(
  sigutils SHARED
  ${SIGUTILS_COMPAT_SOURCES}
  ${SIGUTILS_COMPAT_HEADERS}
  ${SIGUTILS_UTIL_SOURCES}
  ${SIGUTILS_UTIL_HEADERS}
  ${SIGUTILS_LIB_SOURCES}
  ${SIGUTILS_LIB_HEADERS}
  ${SIGUTILS_SPECIFIC_SOURCES})
  
set_property(TARGET sigutils PROPERTY VERSION   ${SIGUTILS_VERSION})
set_property(TARGET sigutils PROPERTY SOVERSION ${SIGUTILS_ABI_VERSION})

# Private header directories
target_include_directories(sigutils PRIVATE . util ${SRCDIR})

# Required dependencies
target_include_directories(sigutils SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(sigutils ${SNDFILE_LIBRARIES})

target_include_directories(sigutils SYSTEM PUBLIC ${FFTW3_INCLUDE_DIRS})
target_link_libraries(sigutils ${FFTW3_LIBRARIES})

target_link_libraries(sigutils ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sigutils m)

if(WIN32)
  # In Windows systems, winsock is required
  target_link_libraries(sigutils ws2_32)
endif()

# Optional dependencies
if(VOLK_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_VOLK=1")
  target_include_directories(sigutils SYSTEM PUBLIC ${VOLK_INCLUDE_DIRS})
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
  
  
############################## Unit tests ######################################
set(MAINDIR src)
set(TESTDIR ${MAINDIR}/tests)
set(TESTUTILDIR testutil)

set(SIGUTILS_TESTUTIL_SOURCES
  ${TESTUTILDIR}/common.c
  ${TESTUTILDIR}/poolhelper.c
  ${TESTUTILDIR}/sigbufpool.c)

set(SIGUTILS_TESTUTIL_HEADERS
  ${TESTUTILDIR}/test.h)
    
set(SIGUTILS_TEST_SOURCES
  ${TESTDIR}/agc.c
  ${TESTDIR}/block.c
  ${TESTDIR}/clock.c
  ${TESTDIR}/costas.c
  ${TESTDIR}/detect.c
  ${TESTDIR}/filt.c
  ${MAINDIR}/main.c
  ${TESTDIR}/mat.c
  ${TESTDIR}/ncqo.c
  ${TESTDIR}/pll.c
  ${TESTDIR}/specttuner.c)
  
set(SIGUTILS_TEST_HEADERS
  ${TESTDIR}/test_list.h
  ${TESTDIR}/test_param.h)
  
add_executable(
  sutest 
  ${SIGUTILS_TEST_SOURCES}
  ${SIGUTILS_TEST_HEADERS}
  ${SIGUTILS_TESTUTIL_SOURCES}
  ${SIGUTILS_TESTUTIL_HEADERS})

# Private header directories
target_include_directories(
  sutest 
  PRIVATE . util ${SRCDIR} ${TESTDIR} ${TESTUTILDIR})

# Required dependencies
target_link_libraries(sutest sigutils)

target_include_directories(sutest SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(sutest ${SNDFILE_LIBRARIES})

target_include_directories(sutest SYSTEM PUBLIC ${FFTW3_INCLUDE_DIRS})
target_link_libraries(sutest ${FFTW3_LIBRARIES})

target_link_libraries(sutest ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sutest m)

# Optional dependencies
if(VOLK_FOUND)
  target_include_directories(sutest SYSTEM PUBLIC ${VOLK_INCLUDE_DIRS})
  target_link_libraries(sutest ${VOLK_LIBRARIES})
endif()
