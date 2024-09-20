#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual VariableRange<ScalableGenerator>
    {
    public:
        void SetType( SimplexType value ) { mType = value; }
        const Metadata& GetMetadata() const override;

    protected:
        SimplexType mType = SimplexType::Standard;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Simplex> : MetadataT<VariableRange<ScalableGenerator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from an N dimensional simplex grid\n"
                "Developed by Ken Perlin in 2001";

            this->AddVariableEnum(
                { "Type", "Noise character style" },
                SimplexType::Standard, &Simplex::SetType,
                kSimplexType_Strings
            );
        }
    };
#endif
}
