# always use this command if CMakeCache.txt isn't created
# cmake -G "MinGW Makefiles" -D CMAKE_MAKE_PROGRAM:FILEPATH=make .

cmake_minimum_required(VERSION 3.20)

project(teascript)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE SOURCE_FILES src/*.c src/debug/*.c src/util/*.c src/scanner/*.c src/parser/*.c src/memory/*.c src/vm/*.c src/types/*.c src/modules/*.c)
include_directories(include/)
add_executable(tea ${SOURCE_FILES})
target_link_libraries(tea LINK_PUBLIC m) # Lib math