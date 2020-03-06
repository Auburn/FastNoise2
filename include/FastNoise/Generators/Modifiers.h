#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainScale : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        void SetScale( float value ) { mScale = value; };

    protected:
        float mScale = 1.0f;
    };

    class Remap : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );

        void SetRemap(float fromMin, float fromMax, float toMin, float toMax) { mFromMin = fromMin; mFromMax = fromMax; mToMin = toMin; mToMax = toMax; }

    protected:
        float mFromMin = -1.0f;
        float mFromMax = 1.0f;
        float mToMin = 0.0f;
        float mToMax = 1.0f;
    };

    class ConvertRGBA8 : public virtual Modifier<1>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT(FastNoise::SUPPORTED_SIMD_LEVELS);

        void SetMinMax(float min, float max) { mMin = min; mMax = max; }

    protected:
        float mMin = -1.0f;
        float mMax = 1.0f;
    };
}
