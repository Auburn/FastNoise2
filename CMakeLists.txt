# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.7.1)

include("cmake/Hunter/HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.23.181.tar.gz"
    SHA1 "270cafa2d67def4ad4d52409ff22a28890d41c84"
    LOCAL)

project(FastNoiseTool)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

hunter_add_package(magnum)
find_package(Magnum CONFIG REQUIRED
    GL
    MeshTools
    Primitives
    Shaders
    Sdl2Application)

add_executable(NoiseTool "NoiseTool/main.cpp")
 
target_include_directories(FastNoise PUBLIC include)
 
target_link_libraries(NoiseTool FastNoise)
add_dependencies(NoiseTool FastNoise)
