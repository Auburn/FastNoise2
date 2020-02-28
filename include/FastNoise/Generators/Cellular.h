#pragma once

#include "Generator.h"

namespace FastNoise
{
    class Cellular : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        enum class DistanceFunction
        {
            Euclidean,
            EuclideanSquared,
            Manhattan,
            Natural,
        };

        void SetJitterModifier( float value ) { mJitterModifier = value; }
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

    protected:
        float mJitterModifier = 1.0f;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;

        const float kJitter2D = 0.5f;
        const float kJitter3D = 0.45f;
    };

    class CellularValue : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
    };

    class CellularDistance : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
    };

    class CellularLookup : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        virtual void SetLookup( const std::shared_ptr<Generator>& gen ) = 0;

        void SetLookupFrequency( float freq ) { mLookupFreqX = freq; mLookupFreqY = freq; mLookupFreqZ = freq; mLookupFreqW = freq; }
        void SetLookupFrequencyAxis( float freqX, float freqY = 0.1f, float freqZ = 0.1f, float freqW = 0.1f )
        {
            mLookupFreqX = freqX; mLookupFreqY = freqY; mLookupFreqZ = freqZ; mLookupFreqW = freqW;
        }

    protected:
        float mLookupFreqX = 0.1f;
        float mLookupFreqY = 0.1f;
        float mLookupFreqZ = 0.1f;
        float mLookupFreqW = 0.1f;

        std::shared_ptr<Generator> mLookupBase;
    };
}
