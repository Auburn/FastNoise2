#pragma once

#include "Generator.h"

namespace FastNoise
{
    class Cellular : public virtual Generator
    {
    public:
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

        FASTNOISE_METADATA_ABSTRACT( FastNoise )
        
            Metadata( const char* className ) : FastNoise::Metadata( className )
            {
                memberVariables.emplace_back( "Jitter Modifier", 1.0f, &SetJitterModifier );
                //memberVariables.emplace_back( "Distance Function", 1, &SetDistanceFunction );
            }
        };
    };

    class CellularValue : public virtual Cellular
    {
        FASTNOISE_METADATA( Cellular )
            using Cellular::Metadata::Metadata;
        };
    };

    class CellularDistance : public virtual Cellular
    {
        FASTNOISE_METADATA( Cellular )
            using Cellular::Metadata::Metadata;
        };
    };

    class CellularLookup : public virtual Cellular
    {
    public:
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

        FASTNOISE_METADATA( Cellular )
        
            Metadata( const char* className ) : Cellular::Metadata( className )
            {
                memberNodes.emplace_back( "Lookup", &SetLookup );

                memberVariables.emplace_back( "Lookup Frequency X", 0.10f,
                    []( CellularLookup* p, float f )
                {
                    p->mLookupFreqX = f;
                });
                
                memberVariables.emplace_back( "Lookup Frequency Y", 0.1f,
                    []( CellularLookup* p, float f )
                {
                    p->mLookupFreqY = f;
                });
                
                memberVariables.emplace_back( "Lookup Frequency Z", 0.1f,
                    []( CellularLookup* p, float f )
                {
                    p->mLookupFreqZ = f;
                });
                
                memberVariables.emplace_back( "Lookup Frequency W", 0.1f,
                    []( CellularLookup* p, float f )
                {
                    p->mLookupFreqW = f;
                });
            }
        };
    };
}
