
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/benchmark EXCLUDE_FROM_ALL)

add_executable(FastNoiseBenchmark 
    "FastNoiseBenchmark.cpp"
)

target_link_libraries(FastNoiseBenchmark PUBLIC
    FastNoise
    benchmark::benchmark
)
 
add_dependencies(FastNoiseBenchmark FastNoise)
