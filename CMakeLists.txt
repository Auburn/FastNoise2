# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required (VERSION 3.8)

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
    FastNoise/FastSIMD/FastSIMD.h
    FastNoise/FastSIMD/FastSIMD_Config.h
    FastNoise/Generators/Fractal.h
    FastNoise/FastSIMD/FS_IntelliSenseHelper.h
    FastNoise/FastSIMD/Internal/FunctionList.h
    FastNoise/Generators/Generator.h
    FastNoise/Generators/Modifiers.h
    FastNoise/FastSIMD/Internal/NEON.h
    FastNoise/Generators/Perlin.h
    FastNoise/FastSIMD/Internal/Scalar.h
    FastNoise/Generators/Simplex.h
    FastNoise/FastSIMD/Internal/SSE.h
    FastNoise/FastSIMD/Internal/TypeList.h
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

target_compile_options(FastNoise PRIVATE "/arch:AVX2")

add_executable (NoiseTool "NoiseTool/main.cpp")

target_link_libraries(NoiseTool FastNoise)
add_dependencies(NoiseTool FastNoise)
