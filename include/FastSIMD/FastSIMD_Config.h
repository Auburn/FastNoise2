#pragma once
#include <cstdint>
#include <cstddef>

#if __cplusplus >= 201703L // C++17
#if __has_include( <memory_resource> )
#include <memory_resource>
#define FASTSIMD_HAS_MEMORY_RESOURCE true
#endif
#endif

#ifndef FASTSIMD_HAS_MEMORY_RESOURCE
#define FASTSIMD_HAS_MEMORY_RESOURCE false
#endif

#if defined(__arm__) || defined(__aarch64__)
#define FASTSIMD_x86 false
#define FASTSIMD_ARM true
#else
#define FASTSIMD_x86 true
#define FASTSIMD_ARM false
#endif

#define FASTSIMD_64BIT (INTPTR_MAX == INT64_MAX)

#define FASTSIMD_COMPILE_SCALAR (!(FASTSIMD_x86 && FASTSIMD_64BIT)) // Don't compile for x86 64bit since CPU is guaranteed SSE2 support 

#define FASTSIMD_COMPILE_SSE    (FASTSIMD_x86 & false) // Not supported
#define FASTSIMD_COMPILE_SSE2   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE3   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSSE3  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE41  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE42  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_AVX    (FASTSIMD_x86 & false) // Not supported
#define FASTSIMD_COMPILE_AVX2   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_AVX512 (FASTSIMD_x86 & true )

#define FASTSIMD_COMPILE_NEON   (FASTSIMD_ARM & true )

#define FASTSIMD_USE_FMA                   true
#define FASTSIMD_CONFIG_GENERATE_CONSTANTS false

