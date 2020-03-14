#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( FastNoise )
        {
            using FastNoise::Metadata::Metadata;
        };    
        
    };
}
