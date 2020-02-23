#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Fractal : public virtual Modifier<1>
    {
    public:
        void SetLacunarity( float value ) { mLacunarity = value; } 
        void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } 
        void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } 

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
    };

    class FractalFBm : public virtual Fractal
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
    };

    class FractalBillow : public virtual Fractal
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
    };

    class FractalRidged : public virtual Fractal
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        
    };

    class FractalRidgedMulti : public virtual Fractal
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
    };
}
