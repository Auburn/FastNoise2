#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename T = Generator>
    class Fractal : public virtual Generator
    {
    public:
        void SetSource( SmartNodeArg<T> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } 
        void SetGain( SmartNodeArg<> gen ) { mGain = 1.0f; this->SetSourceMemberVariable( mGain, gen ); CalculateFractalBounding(); }
        void SetWeightedStrength( float value ) { mWeightedStrength = value; } 
        void SetWeightedStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWeightedStrength, gen ); }
        void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } 
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:
        GeneratorSourceT<T> mSource;
        HybridSource mGain = 0.5f;
        HybridSource mWeightedStrength = 0.0f;

        int32_t mOctaves = 3;
        float mLacunarity = 2.0f;
        float mFractalBounding = 1.0f / 1.75f;

        virtual void CalculateFractalBounding()
        {
            float gain = std::abs( mGain.constant );
            float amp = gain;
            float ampFractal = 1.0f;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                ampFractal += amp;
                amp *= gain;
            }
            mFractalBounding = 1.0f / ampFractal;
        }

        FASTNOISE_METADATA_ABSTRACT( Generator )

            Metadata( const char* className, const char* sourceName = "Source" ) : Generator::Metadata( className )
            {
                groups.push_back( "Fractal" );

                this->AddGeneratorSource( sourceName, &Fractal::SetSource );
                this->AddHybridSource( "Gain", 0.5f, &Fractal::SetGain, &Fractal::SetGain );
                this->AddHybridSource( "Weighted Strength", 0.0f, &Fractal::SetWeightedStrength, &Fractal::SetWeightedStrength );
                this->AddVariable( "Octaves", 3, &Fractal::SetOctaveCount, 2, 16 );
                this->AddVariable( "Lacunarity", 2.0f, &Fractal::SetLacunarity );
            }
        };        
    };

    class FractalFBm : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )        
            using Fractal::Metadata::Metadata;
        };    
    };

    class FractalRidged : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )
            using Fractal::Metadata::Metadata;
        };              
    };

    class FractalPingPong : public virtual Fractal<>
    {
    public:
        void SetPingPongStrength( float value ) { mPingPongStrength = value; }
        void SetPingPongStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPingPongStrength, gen ); }

    protected:
        HybridSource mPingPongStrength = 0.0f;

        FASTNOISE_METADATA( Fractal )

            Metadata( const char* className ) : Fractal::Metadata( className )
            {
                this->AddHybridSource( "Ping Pong Strength", 2.0f, &FractalPingPong::SetPingPongStrength, &FractalPingPong::SetPingPongStrength );
            }
        };    
    };
}
