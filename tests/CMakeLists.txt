CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    VERSION 1.9.5
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BENCHMARK_ENABLE_TESTING OFF"
        "BENCHMARK_ENABLE_WERROR OFF"
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


add_executable(FastNoiseCodeGenTest
    "FastNoiseCodeGenTest.cpp"
)

target_link_libraries(FastNoiseCodeGenTest
    FastNoise
)


# Runtime node trees compared with the same trees compiled to C++ by FastNoise::CodeGen
# The generator runs at build time, and DLL builds on Windows would need export handling for the generated nodes
if(NOT CMAKE_CROSSCOMPILING AND NOT (WIN32 AND BUILD_SHARED_LIBS))
    add_executable(FastNoiseCodeGenBenchmarkGenerator
        "CodeGenBenchmarkGenerator.cpp"
    )

    target_link_libraries(FastNoiseCodeGenBenchmarkGenerator
        FastNoise
    )

    set(codegen_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/codegen")
    set(codegen_generated_trees "${codegen_generated_dir}/CodeGenBenchmarkTrees.generated.inl")

    add_custom_command(
        OUTPUT "${codegen_generated_trees}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${codegen_generated_dir}"
        COMMAND FastNoiseCodeGenBenchmarkGenerator "${codegen_generated_trees}"
        DEPENDS FastNoiseCodeGenBenchmarkGenerator
        COMMENT "Generating C++ for CodeGen benchmark node trees"
    )
    add_custom_target(FastNoiseCodeGenBenchmarkTrees DEPENDS "${codegen_generated_trees}")

    if(NOT FASTNOISE2_STRICT_FP)
        set(codegen_benchmark_relaxed RELAXED)
    endif()

    fastsimd_create_dispatch_library(FastNoiseCodeGenBenchmarkSIMD ${codegen_benchmark_relaxed} SOURCES "CodeGenBenchmarkNodes.inl")
    add_dependencies(FastNoiseCodeGenBenchmarkSIMD FastNoiseCodeGenBenchmarkTrees)
    target_include_directories(FastNoiseCodeGenBenchmarkSIMD PRIVATE "${FastNoise2_SOURCE_DIR}/include" "${codegen_generated_dir}")

    # Match the FastSIMD_FastNoise dispatch library options
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(FastNoiseCodeGenBenchmarkSIMD PRIVATE /GL- /GS- /wd4251 /permissive-)
    elseif(MSVC)
        target_compile_options(FastNoiseCodeGenBenchmarkSIMD PRIVATE /GS-)
    else()
        target_compile_options(FastNoiseCodeGenBenchmarkSIMD PRIVATE -fno-stack-protector -Wno-nan-infinity-disabled)
    endif()

    if(NOT FASTNOISE2_STRICT_FP)
        if(MSVC)
            target_compile_options(FastNoiseCodeGenBenchmarkSIMD PRIVATE /fp:fast)
        else()
            target_compile_options(FastNoiseCodeGenBenchmarkSIMD PRIVATE -ffast-math)
        endif()
    endif()

    add_executable(FastNoiseCodeGenBenchmark
        "FastNoiseCodeGenBenchmark.cpp"
    )

    target_link_libraries(FastNoiseCodeGenBenchmark
        FastNoise
        FastNoiseCodeGenBenchmarkSIMD
        benchmark::benchmark
    )
endif()
