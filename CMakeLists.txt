cmake_minimum_required(VERSION 3.21)
project(VfwFfmpegBridge VERSION 0.9.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib/minhook)
add_library(vfwbrdg-tmaudio SHARED 
    src/tmaudio.cpp
)
target_include_directories(vfwbrdg-tmaudio PRIVATE lib/minhook/include)
target_link_libraries(vfwbrdg-tmaudio PRIVATE 
    minhook 
    dsound.lib 
    user32.lib
)

add_library(vfwbrdg SHARED 
    src/vfwbrdg.cpp
    src/vfwbrdg.def
    src/dialog.rc
    src/BridgeConfig.cpp
    src/CodecState.cpp
    src/CodecSets.cpp
    src/utils.cpp
)
target_link_libraries(vfwbrdg PRIVATE 
    vfw32.lib 
    winmm.lib 
    user32.lib
    comctl32.lib
)

find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_HASH "unknown")
endif()

target_compile_definitions(vfwbrdg PRIVATE 
    PROJECT_VERSION=L"${CMAKE_PROJECT_VERSION}"
    GIT_HASH=L"${GIT_HASH}"
)