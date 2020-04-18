#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename T = Generator>
    class Fractal : public virtual Modifier<T>
    {
    public:
        void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } 
        void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } 
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:
        int32_t mOctaves = 3;
        float mLacunarity = 2.0f;
        float mGain = 0.5f;
        float mFractalBounding = 1.0f / 1.75f;

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

        FASTNOISE_METADATA_ABSTRACT( Modifier<T> )

            Metadata( const char* className ) : Modifier<T>::Metadata( className )
            {
                this->memberVariables.emplace_back( "Octaves", 3, &Fractal::SetOctaveCount, 2, 16 );
                this->memberVariables.emplace_back( "Lacunarity", 2.0f, &Fractal::SetLacunarity );
                this->memberVariables.emplace_back( "Gain", 0.5f, &Fractal::SetGain );
            }
        };        
    };

    class FractalFBm : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )        
            using Fractal::Metadata::Metadata;
        };    
    };

    class FractalBillow : public virtual Fractal<>
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

    class FractalRidgedMulti : public virtual Fractal<>
    {
    public:
        void SetWeightAmplitude( float value ) { mWeightAmp = value; CalculateFractalBounding(); }

    protected:

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
                memberVariables.emplace_back( "Weight Amplitude", 2.0f, &FractalRidgedMulti::SetWeightAmplitude );
            }
        };      
    };
}
