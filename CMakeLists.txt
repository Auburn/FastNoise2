# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.1)

project(FastNoise)
set(CMAKE_CXX_STANDARD 17)

set(FastNoise_sources
    FastNoise/FastSIMD/FastSIMD.cpp
    FastNoise/FastSIMD/FastSIMD_Level_AVX2.cpp
    FastNoise/FastSIMD/FastSIMD_Level_AVX512.cpp
    FastNoise/FastSIMD/FastSIMD_Level_NEON.cpp
    FastNoise/FastSIMD/FastSIMD_Level_Scalar.cpp
    FastNoise/FastSIMD/FastSIMD_Level_SSE2.cpp
    FastNoise/FastSIMD/FastSIMD_Level_SSE3.cpp
    FastNoise/FastSIMD/FastSIMD_Level_SSE41.cpp
    FastNoise/FastSIMD/FastSIMD_Level_SSE42.cpp
    FastNoise/FastSIMD/FastSIMD_Level_SSSE3.cpp
)

set(FastNoise_headers
    FastNoise/FastSIMD/Internal/AVX.h
    FastNoise/FastSIMD/Internal/AVX512.h
    FastNoise/Generators/Cellular.h
    FastNoise/Generators/DomainWarp.h
    FastNoise/Generators/DomainWarpFractal.h
    FastNoise/FastSIMD/Example/Example.h
    FastNoise/FastNoise.h
    FastNoise/FastNoise_Config.h
    FastNoise/FastSIMD/FastSIMD.h
    FastNoise/FastSIMD/FastSIMD_Config.h
    FastNoise/Generators/Fractal.h
    FastNoise/FastSIMD/FS_IntelliSenseHelper.h
    FastNoise/FastSIMD/FunctionList.h
    FastNoise/Generators/Generator.h
    FastNoise/Generators/Modifiers.h
    FastNoise/FastSIMD/Internal/NEON.h
    FastNoise/Generators/Perlin.h
    FastNoise/FastSIMD/Internal/Scalar.h
    FastNoise/Generators/Simplex.h
    FastNoise/FastSIMD/Internal/SSE.h
    FastNoise/FastSIMD/TypeList.h
    FastNoise/Generators/Value.h
    FastNoise/VecN.h
    FastNoise/FastSIMD/Internal/VecTools.h
    FastNoise/Generators/White.h
)

set(FastNoise_inline
    FastNoise/Generators/Cellular.inl
    FastNoise/Generators/DomainWarp.inl
    FastNoise/Generators/DomainWarpFractal.inl
    FastNoise/FastSIMD/Example/Example.inl
    FastNoise/FastNoise_BuildList.inl
    FastNoise/FastSIMD/FastSIMD_BuildList.inl
    FastNoise/Generators/Fractal.inl
    FastNoise/FastSIMD/FS_Class.inl
    FastNoise/Generators/Generator.inl
    FastNoise/Generators/Modifiers.inl
    FastNoise/Generators/Perlin.inl
    FastNoise/Generators/Simplex.inl
    FastNoise/FastSIMD/Internal/SourceBuilder.inl
    FastNoise/Generators/Value.inl
    FastNoise/Generators/White.inl
)

add_library(FastNoise
    ${FastNoise_sources}
    ${FastNoise_headers}
    ${FastNoise_inline}
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(FastNoise PRIVATE "/GL-")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_SSE3.cpp PROPERTIES COMPILE_FLAGS "-msse3")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_SSSE3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_SSE41.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_SSE42.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    set_source_files_properties(FastNoise/FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq")
endif()


add_executable (NoiseTool "NoiseTool/main.cpp")
 
target_link_libraries(NoiseTool FastNoise)
add_dependencies(NoiseTool FastNoise)
