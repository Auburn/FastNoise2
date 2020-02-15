#define FS_SIMD_CLASS FastSIMD::AVX2

#include "FastSIMD.h"

// To compile AVX2 support enable AVX(2) code generation compiler flags for this file
#if FASTSIMD_COMPILE_AVX2 && !defined(__AVX__)
#ifdef __GNUC__
#error To compile AVX add build command "-march=core-avx" on FastSIMD_Level_AVX2.cpp, or change "#define FASTSIMD_COMPILE_AVX2" in FastSIMD_Config.h
#else
#error To compile AVX set C++ code generation to use /arch:AVX on FastSIMD_Level_AVX2.cpp, or change "#define FASTSIMD_COMPILE_AVX2" in FastSIMD_Config.h
#endif
#endif

#include "Internal/SourceBuilder.inl"
