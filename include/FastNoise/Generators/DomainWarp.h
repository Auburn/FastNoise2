#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual ScalableGenerator
    {
    public:
        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetWarpAmplitude( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWarpAmplitude, gen ); }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 

    protected:
        GeneratorSource mSource;
        HybridSource mWarpAmplitude = 50.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarp> : MetadataT<ScalableGenerator>
    {
        MetadataT()
        {
            groups.push_back( "Domain Warp" );
            this->AddGeneratorSource( "Source", &DomainWarp::SetSource );
            this->AddHybridSource( "Warp Amplitude", 50.0f, &DomainWarp::SetWarpAmplitude, &DomainWarp::SetWarpAmplitude );
        }
    };
#endif

    class DomainWarpGradient : public virtual DomainWarp
    {
    public:        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DomainWarpGradient> : MetadataT<DomainWarp>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;
    };
#endif
}
