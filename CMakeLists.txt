cmake_minimum_required(VERSION 3.13)

project(lox-interpreter-bytecode)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

 # Windows(Visual Studio)-specific: enforces the executable to be in ./build, just like in Linux
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${./build}>)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

add_executable(main ${SOURCE_FILES})