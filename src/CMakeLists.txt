set(CMAKE_CXX_STANDARD 17)

CPMAddPackage(
    NAME FastSIMD
    GITHUB_REPOSITORY Auburn/FastSIMD
    GIT_TAG 5ff01e0274dd7605d0b80d4957e3affb04c1178f
    EXCLUDE_FROM_ALL YES
    #OPTIONS
    #    "BUILD_SHARED_LIBS OFF"
)

set(install_targets ${install_targets}
    FastNoise
    FastSIMD
    FastSIMD_FastNoise  
    PARENT_SCOPE)


file(GLOB FastNoise_headers "../include/FastNoise/*.h")
file(GLOB FastNoise_inl "../include/FastNoise/*.inl")
file(GLOB_RECURSE FastNoise_generators_headers "../include/FastNoise/Generators/*.h")
file(GLOB_RECURSE FastNoise_generators_inl "../include/FastNoise/Generators/*.inl")

set(install_fastnoise_headers ${FastNoise_headers} PARENT_SCOPE)
set(install_fastnoise_generators_headers ${FastNoise_generators_headers} PARENT_SCOPE)

list(APPEND FastNoise_headers ${FastNoise_inl})
list(APPEND FastNoise_generators_headers ${FastNoise_generators_inl})

set(FastNoise_source
    FastNoise/Metadata.cpp
    FastNoise/SmartNode.cpp
    FastNoise/FastNoise_C.cpp)

source_group("FastNoise" FILES ${FastNoise_headers})
source_group("FastNoise" FILES ${FastNoise_source})
source_group("FastNoise\\Generators" FILES ${FastNoise_generators_headers})

add_library(FastNoise
    ${FastNoise_headers}
    ${FastNoise_source}
    ${FastNoise_generators_headers}
    ${FastSIMD_headers}
    ${FastSIMD_internal_headers}
    ${FastSIMD_sources}
)

add_library(FastNoise2 ALIAS FastNoise)

target_include_directories(FastNoise PUBLIC 
    $<BUILD_INTERFACE:${FastNoise2_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
   
target_compile_definitions(FastNoise PRIVATE FASTNOISE_EXPORT)

set_target_properties(FastNoise PROPERTIES
    DEBUG_POSTFIX D
    COMPILE_PDB_NAME_DEBUG FastNoiseD)

fastsimd_create_simd_library(FastSIMD_FastNoise "../include/FastNoise/FastNoise_BuildList.inl")

target_include_directories(FastSIMD_FastNoise PRIVATE "../include/")

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(FastNoise PUBLIC FASTNOISE_STATIC_LIB)
endif()

target_link_libraries(FastNoise PUBLIC FastSIMD PRIVATE FastSIMD_FastNoise)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(FastSIMD_FastNoise PRIVATE /GL- /GS- /fp:fast /wd4251)
    
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    if(MSVC)
        target_compile_options(FastSIMD_FastNoise PRIVATE /GL- /GS- /fp:fast)
    else()
        target_compile_options(FastSIMD_FastNoise PRIVATE -ffast-math -fno-stack-protector)        
    endif()

endif()

