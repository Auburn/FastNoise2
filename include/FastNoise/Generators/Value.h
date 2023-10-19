#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Value : public virtual ScalableGenerator
    {
    public:        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Value> : MetadataT<ScalableGenerator>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from N dimensional grid";
        }
    };
#endif
}
