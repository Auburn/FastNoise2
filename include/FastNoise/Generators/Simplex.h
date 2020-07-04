#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )
            using Generator::Metadata::Metadata;
        };            
    };

    class OpenSimplex : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )
            using Generator::Metadata::Metadata;
        };            
    };
}
