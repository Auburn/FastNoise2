#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
        void SetWarpFrequency( float value ) { mWarpFrequency = value; }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 
        float GetWarpFrequency() { return mWarpFrequency; }
        float GetWarpAmplitude() { return mWarpAmplitude; } 

    protected:
        float mWarpFrequency = 0.5f;
        float mWarpAmplitude = 1.0f;

        FASTNOISE_METADATA_ABSTRACT( Modifier<1> )
        {
            using Modifier<1>::Metadata::Metadata;
        };
    };

    class DomainWarpGradient : public virtual DomainWarp
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( DomainWarp )
        {
            using DomainWarp::Metadata::Metadata;
        };        
    };
}
