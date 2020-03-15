#pragma once
#include "Fractal.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpFractalProgressive : public virtual Fractal<DomainWarp>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( Fractal<DomainWarp> )
            using Fractal<DomainWarp>::Metadata::Metadata;
        };    
    };

    class DomainWarpFractalIndependant : public virtual Fractal<DomainWarp>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( Fractal<DomainWarp> )        
            using Fractal<DomainWarp>::Metadata::Metadata;
        };    
    };
}
