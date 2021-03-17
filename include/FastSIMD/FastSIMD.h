#pragma once
#include "FastSIMD_Config.h"

namespace FastSIMD
{
    typedef uint32_t Level_BitFlags;

    enum eLevel : Level_BitFlags
    {
        Level_Null   = 0,       // Uninitilised
        Level_Scalar = 1 <<  0, // 80386 instruction set (Not SIMD)
        Level_SSE    = 1 <<  1, // SSE (XMM) supported by CPU (not testing for O.S. support)
        Level_SSE2   = 1 <<  2, // SSE2
        Level_SSE3   = 1 <<  3, // SSE3
        Level_SSSE3  = 1 <<  4, // Supplementary SSE3 (SSSE3)
        Level_SSE41  = 1 <<  5, // SSE4.1
        Level_SSE42  = 1 <<  6, // SSE4.2
        Level_AVX    = 1 <<  7, // AVX supported by CPU and operating system
        Level_AVX2   = 1 <<  8, // AVX2
        Level_AVX512 = 1 <<  9, // AVX512, AVX512DQ supported by CPU and operating system

        Level_NEON   = 1 << 16, // ARM NEON
    };

    const Level_BitFlags COMPILED_SIMD_LEVELS =
        (FASTSIMD_COMPILE_SCALAR     ? Level_Scalar : 0) |
        (FASTSIMD_COMPILE_SSE        ? Level_SSE    : 0) |
        (FASTSIMD_COMPILE_SSE2       ? Level_SSE2   : 0) |
        (FASTSIMD_COMPILE_SSE3       ? Level_SSE3   : 0) |
        (FASTSIMD_COMPILE_SSSE3      ? Level_SSSE3  : 0) |
        (FASTSIMD_COMPILE_SSE41      ? Level_SSE41  : 0) |
        (FASTSIMD_COMPILE_SSE42      ? Level_SSE42  : 0) |
        (FASTSIMD_COMPILE_AVX        ? Level_AVX    : 0) |
        (FASTSIMD_COMPILE_AVX2       ? Level_AVX2   : 0) |
        (FASTSIMD_COMPILE_AVX512     ? Level_AVX512 : 0) |
        (FASTSIMD_COMPILE_NEON       ? Level_NEON   : 0) ;
    
    // Wrap std::pmr::memory_resource in struct for compatibility with C++11
    struct MemoryResource
    {
        // nullptr will use standard 'new CLASS()'
        constexpr MemoryResource( nullptr_t = nullptr ) noexcept :
            std_pmr_memory_resource( nullptr ) { }

#if __cplusplus >= 201703L // C++17
        constexpr MemoryResource( std::pmr::memory_resource* mr ) noexcept :
            std_pmr_memory_resource( mr ) { }

        operator std::pmr::memory_resource*() const noexcept
        {
            return (std::pmr::memory_resource*)std_pmr_memory_resource;
        }

        std::pmr::memory_resource* operator->() const noexcept
        {
            return *this;
        }
#endif

        void* std_pmr_memory_resource;
    };

    eLevel CPUMaxSIMDLevel();

    template<typename T>
    T* New( eLevel maxSIMDLevel = Level_Null, MemoryResource mr = nullptr );

    template<typename T, eLevel SIMD_LEVEL>
    T* ClassFactory( MemoryResource mr = nullptr );

#define FASTSIMD_LEVEL_SUPPORT( ... ) \
    static const FastSIMD::Level_BitFlags Supported_SIMD_Levels = __VA_ARGS__
};
