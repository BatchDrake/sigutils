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

# Set the ABI version manually
set(SIGUTILS_ABI_VERSION   1)

# Late module imports that depend on project definitions
include(FindPkgConfig)
include(CodeAnalysis)
include(GNUInstallDirs)
include(RelativeFileMacro)
include(PcFileGenerator)

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
file(GLOB_RECURSE SRCS LIST_DIRECTORIES false *.c)
file(GLOB_RECURSE SRCS_WIN LIST_DIRECTORIES false win32-*.c)
if(NOT WIN32)
  list(REMOVE_ITEM SRCS ${SRCS_WIN})
endif()

# Define the library target	
add_library(sigutils SHARED ${SRCS})

# Define relative filename macros
target_add_relative_file_macro(sigutils)

# Add public include directories
target_include_directories(sigutils PUBLIC include/)

# Extra compilation definitions
if(SIGUTILS_SINGLE_PRECISSION)
  target_compile_definitions(sigutils PUBLIC _SU_SINGLE_PRECISION)
endif()

if(VOLK_FOUND)
  target_compile_definitions(sigutils PUBLIC HAVE_VOLK)
endif()

# Target properties
set_property(TARGET sigutils PROPERTY VERSION   ${SIGUTILS_VERSION})
set_property(TARGET sigutils PROPERTY SOVERSION ${SIGUTILS_ABI_VERSION})

# Target dependencies
target_link_libraries(sigutils ${SNDFILE_LIBRARIES})
target_link_libraries(sigutils ${FFTW3_LIBRARIES})
target_link_libraries(sigutils ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sigutils m)

if(WIN32)
  target_link_libraries(sigutils ws2_32)
endif()

if(VOLK_FOUND)
  target_link_libraries(sigutils ${VOLK_LIBRARIES})
endif()

# PC file generation
target_pc_file_generate(sigutils "Digital signal processing utility library")

# File install
install(DIRECTORY include/sigutils DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS sigutils DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${PROJECT_BINARY_DIR}/src/sigutils.pc DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})
