#pragma once
#include "BasicGenerators.h"

namespace FastNoise
{
    class Value : public virtual VariableRange<Seeded<ScalableGenerator>>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Value> : MetadataT<VariableRange<Seeded<ScalableGenerator>>>
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
