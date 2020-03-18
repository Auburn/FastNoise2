#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Perlin : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )
            using Generator::Metadata::Metadata;
        };        
    };
}
