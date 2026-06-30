cmake_minimum_required(VERSION 3.21)
project(VfwFfmpegBridge CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib/minhook)
add_library(tmaudio SHARED 
    src/tmaudio.cpp
)
target_include_directories(tmaudio PRIVATE lib/minhook/include)
target_link_libraries(tmaudio PRIVATE 
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
)
target_link_libraries(vfwbrdg PRIVATE 
    vfw32.lib 
    winmm.lib 
    user32.lib
    comctl32.lib
)