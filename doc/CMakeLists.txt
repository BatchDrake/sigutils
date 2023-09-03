# Check for the required dependencies
find_package(Doxygen)
find_package(Coverxygen)

if (NOT DOXYGEN_FOUND)
    message("Doxygen needs to be installed to generate the documentation")
else()
    # Select doxygen output formats
    set(DOXYGEN_GENERATE_HTML YES)
    if(${Coverxygen_FOUND})
        set(DOXYGEN_GENERATE_XML YES)
    endif()

    # Other doxy options
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_EXTRACT_PACKAGE YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_EXTRACT_LOCAL_METHODS YES)
    set(DOXYGEN_EXTRACT_ANON_NSPACES YES)
    set(DOXYGEN_MACRO_EXPANSION YES)
    get_target_property(DOXYGEN_INCLUDE_PATH sigutils INCLUDE_DIRECTORIES)
    set(DOXYGEN_SKIP_FUNCTION_MACROS NO)

    # Add a Doxygen target
    doxygen_add_docs(doxygen "../src" ".")
endif()

if (DOXYGEN_FOUND AND NOT Coverxygen_FOUND)
    message("Coverxygen needs to be installed to generate the documentation coverage reports")
else()
    coverxygen_add_report(coverxygen_summary "../src" doxygen markdown-summary)
endif()
