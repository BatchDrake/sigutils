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

cmake_minimum_required(VERSION 3.20.0)

# CMake modules search path
file(GLOB MODULE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/*/" LIST_DIRECTORIES true)
list(APPEND CMAKE_MODULE_PATH "${MODULE_DIRS}")

# Check that all required submodules are there
set(
  GIT_CMAKE_SUBMODULES
  cmake-gitversiondetect
  cmake-pcfilegenerator
  cmake-relativefilemacro)

foreach (submodule IN ITEMS ${GIT_CMAKE_SUBMODULES})
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${submodule}/.git")
  message( FATAL_ERROR "Required CMake submodule `${submodule}' not found. This \
  is most likely caused by an incomplete clone from the main repository. Please run:
  $ git submodule update --init --recursive
to clone all required submodules and run CMake again.")
  endif()
endforeach()

# Use git version detect to obtain a project version based on tags
include(GitVersionDetect)
set(SIGUTILS_VERSION_MAJOR ${GITVERSIONDETECT_VERSION_MAJOR})
set(SIGUTILS_VERSION_MINOR ${GITVERSIONDETECT_VERSION_MINOR})
set(SIGUTILS_VERSION_PATCH ${GITVERSIONDETECT_VERSION_PATCH})
set(SIGUTILS_VERSION ${SIGUTILS_VERSION_MAJOR}.${SIGUTILS_VERSION_MINOR}.${SIGUTILS_VERSION_PATCH})

# Define the project
project(
  sigutils
  VERSION ${SIGUTILS_VERSION}
  DESCRIPTION "Small signal processing utility library"
  HOMEPAGE_URL "http://github.org/BatchDrake/sigutils"
  LANGUAGES C CXX)

# Always use top level dir for generated targets to avoid linker problems
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# The library code is in src
add_subdirectory(src)

# The documentation is in doc
add_subdirectory(doc)

# Tests are in the tests dir
include(CTest)
add_subdirectory(tests)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# Run ldconfig for certain Unix systems
if(UNIX AND NOT APPLE)
  install(CODE "exec_program(ldconfig)")
endif()
