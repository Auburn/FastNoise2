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

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                memberVariables.emplace_back( "Jitter Modifier", 1.0f, &SetJitterModifier );
                memberVariables.emplace_back( "Distance Function", DistanceFunction::EuclideanSquared, &SetDistanceFunction, "Euclidean", "Euclidean Squared", "Manhattan", "Natural" );
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

    class CellularLookup : public virtual Modifier<Generator, Cellular>
    {
    public:
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

        FASTNOISE_METADATA( Modifier<Generator, Cellular> )
        
            Metadata( const char* className ) : Modifier<Generator, Cellular>::Metadata( className )
            {
                memberNodes[0].name = "Lookup";

                memberVariables.emplace_back( "Lookup Frequency X", 0.1f,
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
