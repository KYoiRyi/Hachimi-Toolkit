cmake_minimum_required(VERSION 3.10.2)
project(Uma-ProxyPlugin LANGUAGES CXX)

add_library(uma_proxy SHARED
    src/main.cxx
)

target_compile_features(uma_proxy PRIVATE cxx_std_17)

if(ANDROID)
    target_link_libraries(uma_proxy log dl)
endif()

if(MSVC)
    target_compile_options(uma_proxy PRIVATE /EHsc)
else()
    target_compile_options(uma_proxy PRIVATE -fexceptions)
    set_property(TARGET uma_proxy APPEND_STRING PROPERTY LINK_FLAGS " -s")
endif()
