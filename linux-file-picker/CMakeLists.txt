cmake_minimum_required(VERSION 3.14)

project(fbrg-file-picker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NFD_PORTAL ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.3.0
)
FetchContent_MakeAvailable(nfd)

add_executable(fbrg_file_picker main.cpp)
target_link_libraries(fbrg_file_picker PRIVATE nfd)