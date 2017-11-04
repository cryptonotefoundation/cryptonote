set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library")
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables")

add_subdirectory(miniupnpc)
add_subdirectory(gtest)

if(MSVC)
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR} ../version)
    add_subdirectory(rocksDB EXCLUDE_FROM_ALL)
    set_property(TARGET upnpc-static gtest gtest_main rocksdblib PROPERTY FOLDER "external")
elseif(NOT MSVC)
    set_property(TARGET upnpc-static gtest gtest_main PROPERTY FOLDER "external")
    add_custom_target(
       rocksdb
       COMMAND make static_lib
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/
    )
    add_library(rocksdblib STATIC IMPORTED GLOBAL)
    set_target_properties(rocksdblib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/librocksdb.a)
    add_dependencies(rocksdblib rocksdb)
endif()

if(MSVC)
  set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -wd4244 -wd4267")
elseif(NOT MSVC)
  set_property(TARGET upnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-unused-result -Wno-unused-value")
endif()

