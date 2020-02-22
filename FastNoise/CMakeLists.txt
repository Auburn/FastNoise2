# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.1)

project(FastNoise)
set(CMAKE_CXX_STANDARD 17)

set(FastNoise_sources
    FastSIMD/FastSIMD.cpp
    FastSIMD/FastSIMD_Level_AVX2.cpp
    FastSIMD/FastSIMD_Level_AVX512.cpp
    FastSIMD/FastSIMD_Level_NEON.cpp
    FastSIMD/FastSIMD_Level_Scalar.cpp
    FastSIMD/FastSIMD_Level_SSE2.cpp
    FastSIMD/FastSIMD_Level_SSE3.cpp
    FastSIMD/FastSIMD_Level_SSE41.cpp
    FastSIMD/FastSIMD_Level_SSE42.cpp
    FastSIMD/FastSIMD_Level_SSSE3.cpp
)

set(FastNoise_headers
    FastSIMD/Internal/AVX.h
    FastSIMD/Internal/AVX512.h
    Generators/Cellular.h
    Generators/DomainWarp.h
    Generators/DomainWarpFractal.h
    FastSIMD/Example/Example.h
    FastNoise.h
    FastNoise_Config.h
    FastSIMD/FastSIMD.h
    FastSIMD/FastSIMD_Config.h
    Generators/Fractal.h
    FastSIMD/FS_IntelliSenseHelper.h
    FastSIMD/FunctionList.h
    Generators/Generator.h
    Generators/Modifiers.h
    FastSIMD/Internal/NEON.h
    Generators/Perlin.h
    FastSIMD/Internal/Scalar.h
    Generators/Simplex.h
    FastSIMD/Internal/SSE.h
    FastSIMD/TypeList.h
    Generators/Value.h
    VecN.h
    FastSIMD/Internal/VecTools.h
    Generators/White.h
)

set(FastNoise_inline
    Generators/Cellular.inl
    Generators/DomainWarp.inl
    Generators/DomainWarpFractal.inl
    FastSIMD/Example/Example.inl
    FastNoise_BuildList.inl
    FastSIMD/FastSIMD_BuildList.inl
    Generators/Fractal.inl
    FastSIMD/FS_Class.inl
    Generators/Generator.inl
    Generators/Modifiers.inl
    Generators/Perlin.inl
    Generators/Simplex.inl
    FastSIMD/Internal/SourceBuilder.inl
    Generators/Value.inl
    Generators/White.inl
)

add_library(FastNoise
    ${FastNoise_sources}
    ${FastNoise_headers}
    ${FastNoise_inline}
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(FastNoise PRIVATE "/GL-")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE3.cpp PROPERTIES COMPILE_FLAGS "-msse3")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSSE3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE41.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE42.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq")
endif()
