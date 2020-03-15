#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename T = Generator>
    class Fractal : public virtual Modifier<1, T>
    {
    public:
        void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } 
        void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } 
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:

        float mLacunarity = 2.0f;
        float mGain = 0.5f;
        float mFractalBounding = 1.0f / 1.75f;
        int32_t mOctaves = 3;

        virtual void CalculateFractalBounding()
        {
            float amp = mGain;
            float ampFractal = 1.0f;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                ampFractal += amp;
                amp *= mGain;
            }
            mFractalBounding = 1.0f / ampFractal;
        }

        FASTNOISE_METADATA_ABSTRACT( Modifier<1, T> )

            Metadata( const char* className ) : Modifier<1, T>::Metadata( className )
            {
                memberVariables.emplace_back( "Octaves", 3, &SetOctaveCount );
                memberVariables.emplace_back( "Gain", 2.0f, &SetGain );
                memberVariables.emplace_back( "Lacunarity", 0.5f, &SetLacunarity );
            }
        };        
    };

    class FractalFBm : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( Fractal )        
            using Fractal::Metadata::Metadata;
        };      
        
    };

    class FractalBillow : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( Fractal )
            using Fractal::Metadata::Metadata;
        };      
        
    };

    class FractalRidged : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        FASTNOISE_METADATA( Fractal )
            using Fractal::Metadata::Metadata;
        };      
        
    };

    class FractalRidgedMulti : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
        float mWeightAmp = 2.0f;
        float mWeightBounding = 2.0f / 1.75f;

        virtual void CalculateFractalBounding() override
        {
            Fractal::CalculateFractalBounding();

            float weight = 1.0f;
            float totalWeight = weight;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                weight *= mWeightAmp;
                totalWeight += 1.0f / weight;
            }
            mWeightBounding = 2.0f / totalWeight;
        }

        FASTNOISE_METADATA( Fractal )

            Metadata( const char* className ) : Fractal::Metadata( className )
            {

            }
        };      
    };
}
