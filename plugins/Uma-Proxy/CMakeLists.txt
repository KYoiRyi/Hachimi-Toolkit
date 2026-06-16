cmake_minimum_required(VERSION 3.10.2)
project(Uma-ProxyPlugin)

add_library(uma_proxy SHARED
    src/main.cxx
)

target_link_libraries(uma_proxy log dl)
set_target_properties(uma_proxy PROPERTIES CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
