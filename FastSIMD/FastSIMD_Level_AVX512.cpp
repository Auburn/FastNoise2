#define FS_SIMD_CLASS FastSIMD::AVX512

#include "FastSIMD.h"

// To compile AVX2 support enable AVX(2) code generation compiler flags for this file
#if FASTSIMD_COMPILE_AVX512 && !defined(__AVX__)
#ifdef __GNUC__
#error To compile AVX512 add build command "-march=core-avx" on FastSIMD_Level_AVX512.cpp, or change "#define FASTSIMD_COMPILE_AVX512" in FastSIMD_Config.h
#else
#error To compile AVX512 set C++ code generation to use /arch:AVX on FastSIMD_Level_AVX512.cpp, or change "#define FASTSIMD_COMPILE_AVX512" in FastSIMD_Config.h
#endif
#endif

#include "Internal/SourceBuilder.inl"
