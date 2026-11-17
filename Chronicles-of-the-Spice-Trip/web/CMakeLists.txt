# The browser build (iPhone, Android and desktop browsers): the game plus raylib, compiled to
# WebAssembly with Emscripten. From the Chronicles-of-the-Spice-Trip folder:
#
#   emcmake cmake -S web -B build/web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/web
#   python3 -m http.server -d build/web/site 8080      # then open http://localhost:8080
#
# build/web/site/ is the whole site: the page, the game and its packed assets.
cmake_minimum_required(VERSION 3.22)
project(spicetrip_web C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure this with emcmake (Emscripten), see the top of web/CMakeLists.txt")
endif()

include(FetchContent)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
FetchContent_GetProperties(raylib)
if(NOT raylib_POPULATED)
    FetchContent_Populate(raylib)      # sources only: raylib's own CMake isn't used
endif()
set(RL ${raylib_SOURCE_DIR}/src)
set(GAME ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(WEB ${CMAKE_CURRENT_SOURCE_DIR})
set(SITE ${CMAKE_CURRENT_BINARY_DIR}/site)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB GAME_SOURCES ${GAME}/src/*.cpp)
add_executable(index
    ${GAME_SOURCES}
    ${RL}/rcore.c ${RL}/rshapes.c ${RL}/rtextures.c ${RL}/rtext.c
    ${RL}/rmodels.c ${RL}/utils.c ${RL}/raudio.c
    ${WEB}/glfw_compat.c)
set_target_properties(index PROPERTIES SUFFIX ".html" RUNTIME_OUTPUT_DIRECTORY ${SITE})

target_include_directories(index PRIVATE ${GAME}/include ${GAME}/vendor/pl_mpeg ${RL})
target_compile_definitions(index PRIVATE PLATFORM_WEB GRAPHICS_API_OPENGL_ES2)
target_compile_options(index PRIVATE -w)

# The assets are packed into index.data and appear at /assets. Art that is only a source
# for other files (the full-size weapon pictures, unused stock) stays out to keep it smaller.
set(EXCLUDE
    "*/dobrich-stock/*" "*/dobrich-guy/*" "*/item.png" "*/RustySword.png"
    "*/marlboro-pack/pack.png" "*/toshko_source.jpg" "*.txt")
set(EXCLUDE_ARGS)
foreach(pattern ${EXCLUDE})
    list(APPEND EXCLUDE_ARGS "SHELL:--exclude-file \"${pattern}\"")
endforeach()

target_link_options(index PRIVATE
    -sUSE_GLFW=3
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    -sFORCE_FILESYSTEM=1
    -sMINIFY_HTML=0                      # keep the page as written
    -lidbfs.js
    "SHELL:--preload-file ${GAME}/assets@/assets"
    ${EXCLUDE_ARGS}
    "SHELL:--shell-file ${WEB}/shell.html"
    "SHELL:--pre-js ${WEB}/pre.js")
set_property(TARGET index APPEND PROPERTY LINK_DEPENDS ${WEB}/shell.html ${WEB}/pre.js)

# The page's own files sit next to it.
foreach(file poster.jpg icon-180.png icon-192.png icon-512.png manifest.json)
    configure_file(${WEB}/${file} ${SITE}/${file} COPYONLY)
endforeach()
