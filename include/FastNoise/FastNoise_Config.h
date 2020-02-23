#pragma once
#include "FastSIMD/FastSIMD.h"

namespace FastNoise
{
    const FastSIMD::Level_BitFlags SUPPORTED_SIMD_LEVELS =
        FastSIMD::Level_Scalar |
        FastSIMD::Level_SSE2   |
        FastSIMD::Level_SSE41  |
        FastSIMD::Level_AVX2   |
        FastSIMD::Level_AVX512; 
}