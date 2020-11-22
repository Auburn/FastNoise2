
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/benchmark EXCLUDE_FROM_ALL)

add_executable(FastNoiseBenchmark 
    "FastNoiseBenchmark.cpp"
)

target_link_libraries(FastNoiseBenchmark
    FastNoise
    benchmark::benchmark
)
 
add_dependencies(FastNoiseBenchmark FastNoise)


add_executable(FastSIMDTest
    "SIMDUnitTest.cpp"
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "/arch:AVX512")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq -mavx2 -mfma -msse4.2")
endif()

target_link_libraries(FastSIMDTest
    FastNoise
)
 
add_dependencies(FastSIMDTest FastNoise)