# Check for the required dependencies
find_package(Doxygen)

if (NOT DOXYGEN_FOUND)
    message("Doxygen needs to be installed to generate the documentation")
else()
    # Instruct Doxygen to generate HTML output
    set(DOXYGEN_GENERATE_HTML YES)

    # Add a Doxygen target
    doxygen_add_docs(doxygen "../src")
endif()
