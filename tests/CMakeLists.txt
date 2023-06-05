CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    VERSION 1.5.5
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BENCHMARK_ENABLE_TESTING OFF"
)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin CACHE PATH "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib CACHE PATH "" FORCE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib CACHE PATH "" FORCE)
endif()

# Ensure FastNoise.dll is built into the same dir as executables
set_target_properties(FastNoise
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
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

#add_executable(FastSIMDTest
#    "SIMDUnitTest.cpp"
#)
#
#if(NOT FASTSIMD_COMPILE_ARM)
#
#    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#    set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "/arch:AVX512")
#
#    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
#        set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq -mavx2 -mfma -msse4.2")
#    endif()
#
#elseif(FASTSIMD_COMPILE_ARMV7)
#
#    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#    set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "/arch:NEON")
#
#    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
#        set_source_files_properties("SIMDUnitTest.cpp" PROPERTIES COMPILE_FLAGS "-march=armv7-a -mfpu=neon")
#    endif()
#endif()
#
#
#
#target_link_libraries(FastSIMDTest
#    FastNoise
#)
# 
#add_dependencies(FastSIMDTest 
#    FastNoise
#)
