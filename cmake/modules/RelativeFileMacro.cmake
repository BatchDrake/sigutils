# Distributed under the GPL v3 License. 

#[=======================================================================[.rst:
RelativeFileMacro
-------

This module attempts to define the __REL_FILE__ macro as the relative paths
of a file.

This module requires CMake 3.20 or newer.

Custom functions
^^^^^^^^^^^^^^^^

``source_add_relative_file_macro``
  Add a __REL_FILE__ macro definition a source file.
``target_add_relative_file_macro``
  Add a __REL_FILE__ macro definition to all sources in the provided target.


#]=======================================================================]

# Function that sets the __REL_FILE__ definition for a particular source file
function(source_add_relative_file_macro SRC_FILE_VAR)
    # Get the real src file path
    set(SRC_FILE ${${SRC_FILE_VAR}})
    # Test if the provided path is relative
    cmake_path(IS_RELATIVE SRC_FILE_VAR SRC_FILE_IS_REL)
    if(SRC_FILE_IS_REL)
        set(SRC_FILE_REL "\"${SRC_FILE}\"")
    else()
        # Get relative path
        # Handles non normalized paths and mixed platform paths :)
        cmake_path(RELATIVE_PATH ${SRC_FILE_VAR} BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE SRC_FILE_REL)
        set(SRC_FILE_REL "\"${SRC_FILE}\"")
    endif()
    # Set a __FILE__ property for the current source file
    set_property(SOURCE ${SRC_FILE} APPEND PROPERTY COMPILE_DEFINITIONS __REL_FILE__=${SRC_FILE_REL})
endfunction()

# Function that sets the __REL_FILE__ definition in all files of a particular target
function(target_add_relative_file_macro TARGET)
    get_target_property(TARGET_SRCS ${TARGET} SOURCES)
    foreach(SRC ${TARGET_SRCS})
        source_add_relative_file_macro(SRC)
    endforeach()
endfunction()
