# Required packages
find_package(Git)

# Check if a git executable was found
if(GIT_EXECUTABLE)
  # Generate a git-describe version string from Git repository tags
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "v*"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  # If no error took place, save the version
  if(NOT GIT_DESCRIBE_ERROR_CODE)
    set(GITVERSIONDETECT_VERSION ${GIT_DESCRIBE_VERSION})
  endif()
endif()

# Final fallback: Just use a bogus version string that is semantically older
# than anything else and spit out a warning to the developer.
if(NOT DEFINED GITVERSIONDETECT_VERSION)
  set(GITVERSIONDETECT_VERSION v0.0.0-unknown)
  message(WARNING "Failed to determine GITVERSIONDETECT_VERSION from Git tags. Using default version \"${GITVERSIONDETECT_VERSION}\".")
endif()

# Split the version into major, minor, patch and prerelease
string(REGEX MATCH "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([a-zA-Z0-9\\-]+))?"
  GITVERSIONDETECT_VERSION_MATCH ${GITVERSIONDETECT_VERSION})
set(GITVERSIONDETECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(GITVERSIONDETECT_VERSION_MINOR ${CMAKE_MATCH_2})
set(GITVERSIONDETECT_VERSION_PATCH ${CMAKE_MATCH_3})
set(GITVERSIONDETECT_VERSION_PRERELEASE ${CMAKE_MATCH_5})
