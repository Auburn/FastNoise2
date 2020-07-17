set(CMAKE_CXX_STANDARD 17)

set(FastSIMD_sources
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

file(GLOB_RECURSE FastSIMD_headers "*.h")
file(GLOB_RECURSE FastSIMD_inline "*.inl")

add_library(FastNoise
    FastNoise/FastNoiseMetadata.cpp
    ${FastSIMD_sources}
    ${FastSIMD_headers}
    ${FastSIMD_inline}
)

target_include_directories(FastNoise PUBLIC ../include)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(FastNoise PRIVATE /GL- /GS- /fp:fast)
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX512")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
if(MSVC)
    target_compile_options(FastNoise PRIVATE /GS- /fp:fast)
endif()
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE3.cpp PROPERTIES COMPILE_FLAGS "-msse3")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSSE3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE41.cpp PROPERTIES COMPILE_FLAGS "-msse4.1")
    set_source_files_properties(FastSIMD/FastSIMD_Level_SSE42.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX2.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    set_source_files_properties(FastSIMD/FastSIMD_Level_AVX512.cpp PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq")
endif()
