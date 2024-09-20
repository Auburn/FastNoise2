#pragma once
#include "Generator.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpSimplex : public virtual DomainWarp
    {
    public:
        const Metadata& GetMetadata() const override;

        void SetType( SimplexType value ) { mType = value; }
        void SetVectorizationScheme( VectorizationScheme value ) { mVectorizationScheme = value; }

    protected:
        SimplexType mType = SimplexType::Standard;
        VectorizationScheme mVectorizationScheme = VectorizationScheme::OrthogonalGradientMatrix;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpSimplex> : MetadataT<DomainWarp>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            this->AddVariableEnum(
                { "Type", "Noise character style" },
                SimplexType::Standard, &DomainWarpSimplex::SetType,
                kSimplexType_Strings
            );
            this->AddVariableEnum(
                { "Vectorization Scheme", "Construction used by the noise to produce a vector output" },
                VectorizationScheme::OrthogonalGradientMatrix, &DomainWarpSimplex::SetVectorizationScheme,
                kVectorizationScheme_Strings
            );
        }
    };
#endif
}
