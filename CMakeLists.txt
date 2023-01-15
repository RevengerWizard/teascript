cmake_minimum_required(VERSION 3.20)

project(teascript)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O2 -s")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")

file(GLOB_RECURSE SOURCE_FILES src/*.c)
include_directories(src/)
add_executable(tea ${SOURCE_FILES})

target_link_libraries(tea LINK_PUBLIC m) # Lib math