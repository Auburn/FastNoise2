#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Perlin : public virtual ScalableGenerator
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Perlin> : MetadataT<ScalableGenerator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from N dimensional grid\n"
                "Developed by Ken Perlin in 1983";
        }
    };
#endif
}
