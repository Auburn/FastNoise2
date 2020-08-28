# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.7.1)

project(FastNoiseTool)
set(CMAKE_CXX_STANDARD 17)

option(FASTNOISE2_NOISETOOL "Build Noise Tool" ON)
option(FASTNOISE2_TESTS "Build Test" OFF)

add_subdirectory(src)

if(FASTNOISE2_NOISETOOL)
	add_subdirectory(NoiseTool)
endif()

if(FASTNOISE2_TESTS)
	add_subdirectory(tests)
endif()
