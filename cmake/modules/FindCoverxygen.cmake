# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2023 Antonio Vázquez Blanco <antoniovazquezblanco@gmail.com>

# This package uses FindPython3, set min ver acordingly
cmake_minimum_required(VERSION 3.12.0)

# Check if python is available...
find_package(Python3 COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    # Python found. Try to find the module...
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import coverxygen"
        RESULT_VARIABLE EXIT_CODE
        OUTPUT_QUIET
    )
    if (${EXIT_CODE} EQUAL 0)
        set(Coverxygen_FOUND ON)
        set(Coverxygen_COMMAND "${Python3_EXECUTABLE} -m coverxygen")
    endif()
endif()

if (NOT Coverxygen_FOUND)
    message("Coverxygen not found")
endif()

function(coverxygen_add_report targetName srcDir doxygenTarget reportFormat)
    get_filename_component(absSrcDir "${srcDir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(${targetName} VERBATIM
        COMMAND "${Python3_EXECUTABLE}" -m coverxygen --xml-dir "${CMAKE_CURRENT_BINARY_DIR}/xml" --src-dir "${absSrcDir}" --output "${CMAKE_CURRENT_BINARY_DIR}/doc-coverage.${reportFormat}.info" --format ${reportFormat}
        DEPENDS ${doxygenTarget}
        COMMENT "Generate documentation coverage report in ${reportFormat} for ${targetName}"
    )
endfunction()
