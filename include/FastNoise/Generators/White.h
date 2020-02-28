#pragma once
#include "Generator.h"

namespace FastNoise
{
    class White : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
    };
}
