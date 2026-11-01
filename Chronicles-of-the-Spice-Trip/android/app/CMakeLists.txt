# The game plus raylib, built together into the library NativeActivity loads.
cmake_minimum_required(VERSION 3.22)
project(spicetrip C CXX)

include(FetchContent)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
FetchContent_GetProperties(raylib)
if(NOT raylib_POPULATED)
    FetchContent_Populate(raylib)      # sources only: raylib's own CMake isn't used
endif()
set(RL ${raylib_SOURCE_DIR}/src)
set(GLUE ${ANDROID_NDK}/sources/android/native_app_glue)
set(GAME ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

file(GLOB GAME_SOURCES ${GAME}/src/*.cpp)
add_library(spicetrip SHARED
    ${GAME_SOURCES}
    ${RL}/rcore.c ${RL}/rshapes.c ${RL}/rtextures.c ${RL}/rtext.c
    ${RL}/rmodels.c ${RL}/utils.c ${RL}/raudio.c
    ${GLUE}/android_native_app_glue.c)

target_include_directories(spicetrip PRIVATE ${GAME}/include ${RL} ${GLUE})
target_compile_definitions(spicetrip PRIVATE PLATFORM_ANDROID GRAPHICS_API_OPENGL_ES2)
target_compile_options(spicetrip PRIVATE -w)
target_link_options(spicetrip PRIVATE -u ANativeActivity_onCreate -Wl,-z,max-page-size=16384)
target_link_libraries(spicetrip PRIVATE android log EGL GLESv2 OpenSLES m)
