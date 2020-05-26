#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual Generator
    {
    public:
        void SetSource( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetWarpFrequency( float value ) { mWarpFrequency = value; }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 
        float GetWarpFrequency() const { return mWarpFrequency; }
        float GetWarpAmplitude() const { return mWarpAmplitude; } 

    protected:
        GeneratorSource mSource;
        float mWarpFrequency = 0.5f;
        float mWarpAmplitude = 1.0f;

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddGeneratorSource( "Source", &DomainWarp::SetSource );
                this->AddVariable( "Warp Frequency", 0.5f, &DomainWarp::SetWarpFrequency );
                this->AddVariable( "Warp Amplitude", 1.0f, &DomainWarp::SetWarpAmplitude );
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
