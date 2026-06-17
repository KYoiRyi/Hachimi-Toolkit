cmake_minimum_required(VERSION 3.10.2)
project(Tester-HookerPlugin)

add_library(tester_hooker SHARED
    src/main.cxx
)

target_link_libraries(tester_hooker log dl)
set_target_properties(tester_hooker PROPERTIES CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
