#pragma once
#include "Generator.h"
#include "DomainWarp.h"

namespace FastNoise
{
    enum class VectorizationScheme
    {
        OrthogonalGradientMatrix,
        GradientOuterProduct
    };

    constexpr static const char* kVectorizationScheme_Strings[] =
    {
        "Orthogonal Gradient Matrix",
        "Gradient Outer Product",
    };

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

            description =
                "Warp the domain using a simplex grid\n"
                "The warped position is used when generating the attached source node\n"
                "This node does not change the output value of the source node\n"
                "A higher quality domain warp compared to DomainWarpGradient at the cost of performance";
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
            description =
                "Warp the domain using a smooth simplex grid\n"
                "The warped position is used when generating the attached source node\n"
                "This node does not change the output value of the source node\n"
                "A higher quality smoother warp compared to DomainWarpSimplex at the cost of performance";
        }
    };
#endif
}
