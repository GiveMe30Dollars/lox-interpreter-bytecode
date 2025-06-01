cmake_minimum_required(VERSION 3.13)

project(project-name)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

 # Windows(Visual Studio)-specific: enforces the executable to be in ./build, just like in Linux
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${./build}>)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

add_executable(main ${SOURCE_FILES})