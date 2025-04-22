#pragma once
#include "Generator.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpSimplex : public virtual DomainWarp
    {
    public:
        const Metadata& GetMetadata() const override;
        
        void SetVectorizationScheme( VectorizationScheme value ) { mVectorizationScheme = value; }

    protected:
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
                { "Vectorization Scheme", "Construction used by the noise to produce a vector output" },
                VectorizationScheme::OrthogonalGradientMatrix, &DomainWarpSimplex::SetVectorizationScheme,
                kVectorizationScheme_Strings
            );
        }
    };
#endif

    class DomainWarpSuperSimplex : public virtual DomainWarpSimplex
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpSuperSimplex> : MetadataT<DomainWarpSimplex>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
        }
    };
#endif
}
