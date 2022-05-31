cmake_minimum_required(VERSION 3.20)

project(teascript)

set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_BUILD_TYPE Release)

file(GLOB_RECURSE SOURCE_FILES src/*.c)
include_directories(src/)
add_executable(tea ${SOURCE_FILES})

if(WIN32)
    target_link_libraries(tea LINK_PUBLIC ws2_32 bcrypt) # winsock2.h
endif()

target_link_libraries(tea LINK_PUBLIC m) # Lib math