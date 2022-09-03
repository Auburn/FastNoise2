#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Perlin : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Perlin> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );
        }
    };
#endif
}
