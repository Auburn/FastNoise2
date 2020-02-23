# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.1)

project(FastNoiseTool)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

add_executable (NoiseTool "NoiseTool/main.cpp")
 
target_include_directories(FastNoise PUBLIC include)
 
target_link_libraries(NoiseTool FastNoise)
add_dependencies(NoiseTool FastNoise)
