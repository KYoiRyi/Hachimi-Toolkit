cmake_minimum_required(VERSION 3.10.2)
project(IL2CPPDumperPlugin)

add_library(il2cpp_dumper SHARED
    Dump/src/dumper.cxx
    Dump/src/il2cpp_api.cxx
    Dump/src/il2cpp_class.cxx
    Dump/src/il2cpp_image.cxx
    Dump/src/main.cxx
    Dump/src/scene_dumper.cxx
    Dump/src/utils.cxx
)

target_include_directories(il2cpp_dumper PRIVATE Dump/include)
target_compile_options(il2cpp_dumper PRIVATE -include win32_compat.hxx)
target_link_libraries(il2cpp_dumper log dl)
set_target_properties(il2cpp_dumper PROPERTIES CXX_STANDARD 17)
