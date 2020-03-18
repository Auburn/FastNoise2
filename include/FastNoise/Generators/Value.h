#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Value : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )
            using Generator::Metadata::Metadata;
        };            
    };
}
