#pragma once
#include "Fractal.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpFractalProgressive : public virtual Fractal<DomainWarp>
    {
        FASTNOISE_METADATA( Fractal<DomainWarp> )
            using Fractal<DomainWarp>::Metadata::Metadata;
        };    
    };

    class DomainWarpFractalIndependant : public virtual Fractal<DomainWarp>
    {
        FASTNOISE_METADATA( Fractal<DomainWarp> )        
            using Fractal<DomainWarp>::Metadata::Metadata;
        };    
    };
}
