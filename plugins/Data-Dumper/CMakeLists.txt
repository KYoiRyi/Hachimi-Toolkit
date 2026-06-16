cmake_minimum_required(VERSION 3.10.2)
project(DataDumperPlugin)

add_library(data_dumper SHARED
    src/main.cxx
)

target_link_libraries(data_dumper log dl)
set_target_properties(data_dumper PROPERTIES CXX_STANDARD 17)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
