cmake_minimum_required(VERSION 3.10.2)
project(PacketCapturePlugin)

add_library(packet_capture SHARED
    src/main.cxx
)

target_link_libraries(packet_capture log dl)
set_target_properties(packet_capture PROPERTIES CXX_STANDARD 17)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
