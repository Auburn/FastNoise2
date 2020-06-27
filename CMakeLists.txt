# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.12)

project(FastNoiseTool)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

add_subdirectory(tests)
add_subdirectory(NoiseTool)
