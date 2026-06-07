# list include header files in Visual Studio Solution Explorer

file(GLOB_RECURSE IncludeHeaders "${CMAKE_SOURCE_DIR}/include/*.h*")
add_custom_target(include SOURCES ${IncludeHeaders})
source_group("Public Headers" FILES ${IncludeHeaders})