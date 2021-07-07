# Read project version from git tag.
cmake_policy(SET CMP0007 OLD)
find_package(Git)
message(STATUS "Found Git: ${GIT_FOUND}")
if(GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match "v*"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE PROJECT_GIT_RESULT
    OUTPUT_VARIABLE PROJECT_GIT_TAG
    ERROR_VARIABLE PROJECT_GIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT PROJECT_GIT_RESULT EQUAL 0)
    set(PROJECT_GIT_TAG "0.0.0")
	set(PROJECT_VERSION_MAJOR 0)
	set(PROJECT_VERSION_MINOR 0)
	set(PROJECT_VERSION_PATCH 0)
    message(WARNING "Failed to execute Git: ${PROJECT_GIT_ERROR}")
  else()
    string(REPLACE "v." "" PROJECT_GIT_TAG ${PROJECT_GIT_TAG})

    # Parse and split a version number into separate components
	string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)?" "\\1;\\2;\\3;\\4" VERSION_MATCHES ${PROJECT_GIT_TAG})
    list(GET VERSION_MATCHES 0 version_major)
    set(PROJECT_VERSION_MAJOR ${version_major})
    list(GET VERSION_MATCHES 1 version_minor)
    set(PROJECT_VERSION_MINOR ${version_minor})
    list(GET VERSION_MATCHES 2 version_patch)
    set(PROJECT_VERSION_PATCH ${version_patch})

    message(STATUS "Git tag: ${PROJECT_GIT_TAG}")
    message(STATUS "VERSION_MAJOR: ${PROJECT_VERSION_MAJOR}")
    message(STATUS "VERSION_MINOR: ${PROJECT_VERSION_MINOR}")
    message(STATUS "VERSION_PATCH: ${PROJECT_VERSION_PATCH}")
  endif()

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE PROJECT_GIT_RESULT
    OUTPUT_VARIABLE PROJECT_GIT_COMMIT_ID
    ERROR_VARIABLE PROJECT_GIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT PROJECT_GIT_RESULT EQUAL 0)
    set(PROJECT_GIT_COMMIT_ID "unknown")
    message(WARNING "Failed to execute Git: ${PROJECT_GIT_ERROR}")
  else()
    message(STATUS "Git commit ID: ${PROJECT_GIT_COMMIT_ID}")
  endif()

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT PROJECT_GIT_RESULT EQUAL 0)
    message(WARNING "Failed to execute Git: ${PROJECT_GIT_ERROR}")
	set(PROJECT_VERSION_REV 0)   
  else()
    message(STATUS "Git commit count: ${PROJECT_GIT_COMMIT_COUNT}")
	set(PROJECT_VERSION_REV ${PROJECT_GIT_COMMIT_COUNT})
  endif()

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in" "${TO}")
endif()