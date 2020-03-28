
hunter_add_package(benchmark)
find_package(benchmark CONFIG REQUIRED)

add_executable(FastNoiseBenchmark 
    "FastNoiseBenchmark.cpp"
)

target_link_libraries(FastNoiseBenchmark PUBLIC
    FastNoise
    benchmark::benchmark
)
 
add_dependencies(FastNoiseBenchmark FastNoise)
