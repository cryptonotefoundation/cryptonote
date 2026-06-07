message(STATUS "Building LMDB in-tree for blockchain storage")

cmake_minimum_required(VERSION 3.8)

project(lmdb LANGUAGES C CXX)

set (lmdb_sources
mdb.c
midl.c)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_library(lmdb
  ${lmdb_sources})
target_link_libraries(lmdb)

set_property(TARGET lmdb APPEND PROPERTY COMPILE_FLAGS "-fPIC")

string(TOUPPER "${PROJECT_NAME}" setting_prefix)
function(get_setting setting_name setting_type setting_description)
    string(TOUPPER "${setting_prefix}_${setting_name}" setting_external_name)
    set("${setting_external_name}" "" CACHE "${setting_type}" "${setting_description}")
    set("${setting_name}" "${${setting_external_name}}" PARENT_SCOPE)
endfunction()


get_setting(target_arch STRING "Target system architecture (fed to the compiler's -march=...).")
if(NOT target_arch AND NOT CMAKE_CROSSCOMPILING)
    set(target_arch native)
endif()


# Main Library

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_sources("${PROJECT_NAME}" PRIVATE ${project_source_files})

target_include_directories("${PROJECT_NAME}" PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

target_compile_options("${PROJECT_NAME}" PRIVATE ${compile_options})

# Installation

set(targets_export_name "${PROJECT_NAME}-targets")

install(TARGETS "${PROJECT_NAME}"
        EXPORT "${targets_export_name}"
        LIBRARY DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
        ARCHIVE DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

install(FILES ${project_header_files}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# CMake find_package() support

export(PACKAGE "${PROJECT_NAME}")


