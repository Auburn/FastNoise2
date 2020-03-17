#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
        FASTNOISE_METADATA( FastNoise )
            using FastNoise::Metadata::Metadata;
        };            
    };
}
