cmake_minimum_required(VERSION 3.24)
project(SpyroMenuVoices VERSION 0.5.2 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "SpyroMenuVoices requires Windows x64 and MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(minhook
  URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.tar.gz
  URL_HASH SHA256=1aebeae4ca898330c507860acc2fca2eb335fe446a3a2b8444c3bf8b2660a14e)
FetchContent_MakeAvailable(minhook)

add_library(SpyroMenuVoices SHARED
  src/audio_engine.cpp
  src/dllmain.cpp
  src/menu_event_hook.cpp
  src/resources.rc
)
target_include_directories(SpyroMenuVoices PRIVATE include src)
target_compile_definitions(SpyroMenuVoices PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(SpyroMenuVoices PRIVATE /W4 /permissive- /EHsc)
target_link_libraries(SpyroMenuVoices PRIVATE winmm minhook)
set_target_properties(SpyroMenuVoices PROPERTIES PREFIX "" SUFFIX ".asi")
