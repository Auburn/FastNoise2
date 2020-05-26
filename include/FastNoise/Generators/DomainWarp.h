#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetWarpAmplitude( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mWarpAmplitude, gen ); }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 
        void SetWarpFrequency( float value ) { mWarpFrequency = value; }

    protected:
        GeneratorSource mSource;
        HybridSource mWarpAmplitude = 1.0f;
        float mWarpFrequency = 0.5f;

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddGeneratorSource( "Source", &DomainWarp::SetSource );
                this->AddHybridSource( "Warp Amplitude", 1.0f, &DomainWarp::SetWarpAmplitude, &DomainWarp::SetWarpAmplitude );
                this->AddVariable( "Warp Frequency", 0.5f, &DomainWarp::SetWarpFrequency );
            }
        };
    };

    class DomainWarpGradient : public virtual DomainWarp
    {
        FASTNOISE_METADATA( DomainWarp )
            using DomainWarp::Metadata::Metadata;
        };        
    };
}
