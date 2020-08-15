#
#  CMakeLists.txt: CMake configuration file for sigutils
#
#  Copyright (C) 2019 Gonzalo José Carracedo Carballal
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as
#  published by the Free Software Foundation, either version 3 of the
#  License, or (at your option) any later version.
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
  
cmake_minimum_required(VERSION 3.5.1)

set(SIGUTILS_VERSION_MAJOR 0)
set(SIGUTILS_VERSION_MINOR 1)
set(SIGUTILS_VERSION_PATCH 1)

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

# Source location
set(SRCDIR   sigutils)
set(UTILDIR  util)
set(CODECDIR ${SRCDIR}/codecs)
set(BLOCKDIR ${SRCDIR}/blocks)
set(MODEMDIR ${SRCDIR}/modems)

# Compiler configuration
set(SIGUTILS_CONFIG_CFLAGS   "-D_SU_SINGLE_PRECISION")
if(VOLK_FOUND)
  set(SIGUTILS_CONFIG_CFLAGS "${SIGUTILS_CONFIG_CFLAGS} -DHAVE_VOLK=1")
  link_directories(${VOLK_LIBRARY_DIRS})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfo
MinSizeRel."
       FORCE )
endif()

set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} ${SIGUTILS_CONFIG_CFLAGS}")

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
  "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" 
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

set(SIGUTILS_LIB_HEADERS
    ${SRCDIR}/agc.h
    ${SRCDIR}/block.h
    ${SRCDIR}/clock.h
    ${SRCDIR}/codec.h
    ${SRCDIR}/coef.h
    ${SRCDIR}/decider.h
    ${SRCDIR}/detect.h
    ${SRCDIR}/equalizer.h
    ${SRCDIR}/iir.h
    ${SRCDIR}/lfsr.h
    ${SRCDIR}/log.h
    ${SRCDIR}/matfile.h
    ${SRCDIR}/modem.h
    ${SRCDIR}/ncqo.h
    ${SRCDIR}/pll.h
    ${SRCDIR}/property.h
    ${SRCDIR}/sampling.h
    ${SRCDIR}/sigutils.h
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
    ${SRCDIR}/codec.c
    ${SRCDIR}/coef.c
    ${SRCDIR}/detect.c
    ${SRCDIR}/equalizer.c
    ${SRCDIR}/iir.c
    ${SRCDIR}/lfsr.c
    ${SRCDIR}/lib.c
    ${SRCDIR}/log.c
    ${SRCDIR}/matfile.c
    ${SRCDIR}/modem.c
    ${SRCDIR}/ncqo.c
    ${SRCDIR}/pll.c
    ${SRCDIR}/property.c
    ${SRCDIR}/softtune.c
    ${SRCDIR}/specttuner.c
    ${SRCDIR}/taps.c
    ${SRCDIR}/tvproc.c
    ${SRCDIR}/version.c)
    
set(SIGUTILS_BLOCK_SOURCES
    ${BLOCKDIR}/agc.c
    ${BLOCKDIR}/clock.c
    ${BLOCKDIR}/pll.c
    ${BLOCKDIR}/tuner.c
    ${BLOCKDIR}/filt.c
    ${BLOCKDIR}/siggen.c
    ${BLOCKDIR}/wavfile.c)

set(SIGUTILS_CODEC_SOURCES ${CODECDIR}/diff.c)
set(SIGUTILS_MODEM_SOURCES ${MODEMDIR}/qpsk.c)

link_directories(${PROJECT_BINARY_DIR} ${SNDFILE_LIBRARY_DIRS} ${FFTW3_LIBRARY_DIRS})
 
add_library(
  sigutils SHARED
  ${SIGUTILS_UTIL_SOURCES}
  ${SIGUTILS_UTIL_HEADERS}
  ${SIGUTILS_LIB_SOURCES}
  ${SIGUTILS_LIB_HEADERS}
  ${SIGUTILS_BLOCK_SOURCES}
  ${SIGUTILS_CODEC_SOURCES}
  ${SIGUTILS_MODEM_SOURCES})
  
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
  FILES ${SIGUTILS_UTIL_HEADERS} 
  DESTINATION include/sigutils/util)
    
install(TARGETS sigutils DESTINATION lib)
  
  
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
  ${TESTDIR}/ncqo.c
  ${TESTDIR}/codec.c
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
