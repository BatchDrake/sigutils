find_package(Catch2)

if(Catch2_FOUND)
    # General options
    file(GLOB_RECURSE TEST_SRCS LIST_DIRECTORIES false *.cpp)
    add_executable(sigutils_test EXCLUDE_FROM_ALL ${TEST_SRCS})
    target_include_directories(sigutils_test PUBLIC $<TARGET_PROPERTY:sigutils,INCLUDE_DIRECTORIES>)
    target_compile_definitions(sigutils_test PUBLIC $<TARGET_PROPERTY:sigutils,COMPILE_DEFINITIONS>)
    target_link_libraries(sigutils_test $<TARGET_LINKER_FILE:sigutils>)
    include(Catch)
    catch_discover_tests(sigutils_test)

    if(Catch2_VERSION_MAJOR EQUAL 2)
        target_compile_definitions(sigutils_test PUBLIC CATCH2_V2)
        target_link_libraries(sigutils_test Catch2::Catch2)
    elseif(Catch2_VERSION_MAJOR EQUAL 3)
        target_compile_definitions(sigutils_test PUBLIC CATCH2_V3)
        target_link_libraries(sigutils_test Catch2::Catch2WithMain)
    else()
        message("Unsuported Catch2 version")
    endif()
else()
    message("Catch2 needs to be installed to perform unit testing")
endif()
