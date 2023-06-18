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

    # Add a Doxygen target
    doxygen_add_docs(doxygen "../src" ".")
endif()

if (DOXYGEN_FOUND AND NOT Coverxygen_FOUND)
    message("Coverxygen needs to be installed to generate the documentation coverage reports")
else()
    coverxygen_add_report(coverxygen_summary "../src" doxygen summary)
endif()
