cmake_minimum_required(VERSION 3.10.2)
project(ErrorLoggerPlugin)

add_library(error_logger SHARED
    src/main.cxx
)

target_link_libraries(error_logger log dl)
set_target_properties(error_logger PROPERTIES CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
