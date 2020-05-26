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

        void SetJitterModifier( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mJitterModifier, gen ); }
        void SetJitterModifier( float value ) { mJitterModifier = value; }
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

    protected:
        HybridSource mJitterModifier = 1.0f;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;

        const float kJitter2D = 0.5f;
        const float kJitter3D = 0.45f;

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                this->AddHybridSource( "Jitter Modifier", 1.0f, &Cellular::SetJitterModifier, &Cellular::SetJitterModifier );
                this->AddVariableEnum( "Distance Function", DistanceFunction::EuclideanSquared, &Cellular::SetDistanceFunction, "Euclidean", "Euclidean Squared", "Manhattan", "Natural" );
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
        void SetLookup( const std::shared_ptr<Generator>& gen ) { this->SetSourceMemberVariable( mLookup, gen ); }
        void SetLookupFrequency( float freq ) { mLookupFreq = freq; }

    protected:
        GeneratorSource mLookup;
        float mLookupFreq = 0.1f;

        FASTNOISE_METADATA( Cellular )
        
            Metadata( const char* className ) : Cellular::Metadata( className )
            {
                this->AddGeneratorSource( "Lookup", &CellularLookup::SetLookup );
                this->AddVariable( "Lookup Frequency", 0.1f, &CellularLookup::SetLookupFrequency );
            }
        };
    };
}
