#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual SingleSource<>
    {
    public:        
        void SetWarpFrequency( float value ) { mWarpFrequency = value; }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 
        float GetWarpFrequency() { return mWarpFrequency; }
        float GetWarpAmplitude() { return mWarpAmplitude; } 

    protected:
        float mWarpFrequency = 0.5f;
        float mWarpAmplitude = 1.0f;

        FASTNOISE_METADATA_ABSTRACT( SingleSource<> )
        
            Metadata( const char* className ) : SingleSource<>::Metadata( className )
            {
                memberVariables.emplace_back( "Warp Frequency", 0.5f, &DomainWarp::SetWarpFrequency );
                memberVariables.emplace_back( "Warp Amplitude", 1.0f, &DomainWarp::SetWarpAmplitude );
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
