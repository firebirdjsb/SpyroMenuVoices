cmake_minimum_required(VERSION 3.24)
project(SpyroMenuVoices VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "SpyroMenuVoices requires Windows x64 and MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(SpyroMenuVoices SHARED src/dllmain.cpp src/resources.rc)
target_include_directories(SpyroMenuVoices PRIVATE include src)
target_compile_definitions(SpyroMenuVoices PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(SpyroMenuVoices PRIVATE /W4 /permissive- /EHsc)
target_link_libraries(SpyroMenuVoices PRIVATE winmm)
set_target_properties(SpyroMenuVoices PROPERTIES SUFFIX ".asi")
