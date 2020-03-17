#pragma once
#include "Generator.h"

namespace FastNoise
{
    class White : public virtual Generator
    {
        FASTNOISE_METADATA( FastNoise )
            using FastNoise::Metadata::Metadata;
        };            
    };
}
