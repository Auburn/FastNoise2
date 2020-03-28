# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.7.1)

set(HUNTER_ENABLED ON CACHE BOOL "" FORCE)
include("cmake/Hunter/HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.23.181.tar.gz"
    SHA1 "270cafa2d67def4ad4d52409ff22a28890d41c84"
    LOCAL)

project(FastNoiseTool)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

add_subdirectory(tests)
add_subdirectory(NoiseTool)
