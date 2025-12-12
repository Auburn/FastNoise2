#pragma once
#include "BasicGenerators.h"

namespace FastNoise
{
    class DomainWarp : public virtual Seeded<ScalableGenerator>
    {
    public:
        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetWarpAmplitude( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWarpAmplitude, gen ); }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 

        template<Dim D>
        void SetAmplitudeScaling( float value ) { mAxisScale[(int)D] = value; }

    protected:
        GeneratorSource mSource;
        HybridSource mWarpAmplitude = 50.0f;
        PerDimensionVariable<float> mAxisScale = 1.0f;

        friend struct MetadataT<DomainWarp>;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarp> : MetadataT<Seeded<ScalableGenerator>>
    {
        MetadataT()
        {
            groups.push_back( "Domain Warp" );
            this->AddGeneratorSource( "Source", &DomainWarp::SetSource );
            this->AddHybridSource( { "Warp Amplitude", "Maximum (euclidean) distance the position can be moved from it's original location" }, 50.0f, &DomainWarp::SetWarpAmplitude, &DomainWarp::SetWarpAmplitude, 0.1f );

            this->AddPerDimensionVariable( "Amplitude Scaling", 1.0f, []( DomainWarp* p ) { return std::ref( p->mAxisScale ); } );
        }
    };
#endif

    class DomainWarpGradient : public virtual DomainWarp
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpGradient> : MetadataT<DomainWarp>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            description =
                "Warps the input position using a simple uniform grid gradient, similar to perlin noise gradients.\n"
                "The warped position is used when generating the attached source node\n"
                "This node does not change the output value of the source node";
        }
    };
#endif
}
