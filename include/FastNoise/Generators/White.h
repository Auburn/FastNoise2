#pragma once
#include "Generator.h"

namespace FastNoise
{
    class White : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )
            using Generator::Metadata::Metadata;
        };            
    };
}
