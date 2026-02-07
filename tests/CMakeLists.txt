CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    VERSION 1.5.5
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BENCHMARK_ENABLE_TESTING OFF"
)

add_executable(FastNoiseBenchmark 
    "FastNoiseBenchmark.cpp"
)

target_link_libraries(FastNoiseBenchmark
    FastNoise
    benchmark::benchmark
)


add_executable(FastNoiseCpp11Test
    "FastNoiseCpp11Include.cpp"
)

set_target_properties(FastNoiseCpp11Test PROPERTIES CXX_STANDARD 11)

target_link_libraries(FastNoiseCpp11Test
    FastNoise
)
