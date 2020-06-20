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

    class OpenSimplex : public virtual Simplex
    {
        FASTNOISE_METADATA( Simplex )
            using Simplex::Metadata::Metadata;
        };            
    };
}
