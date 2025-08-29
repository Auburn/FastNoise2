#include "Fractal.h"

template<FastSIMD::FeatureSet SIMD, typename T>
class FastSIMD::DispatchClass<FastNoise::Fractal<T>, SIMD> : public virtual FastNoise::Fractal<T>, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{

};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::FractalFBm, SIMD> final : public virtual FastNoise::FractalFBm, public FastSIMD::DispatchClass<FastNoise::Fractal<>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v gain = this->GetSourceValue( mGain  , seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( 1.0f );
        float32v noise = this->GetSourceValue( mSource, seed, pos... );

        float32v sum = noise * amp;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= Lerp( float32v( 1 ), (noise + float32v( 1 )) * float32v( 0.5f ), weightedStrength );
            amp *= gain;

            noise = this->GetSourceValue( mSource, seed, (pos *= lacunarity)... );
            sum += noise * amp;
        }

        return sum;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::FractalRidged, SIMD> final : public virtual FastNoise::FractalRidged, public FastSIMD::DispatchClass<FastNoise::Fractal<>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT(int32v seed, P... pos) const
    {
        float32v gain = this->GetSourceValue( mGain, seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( 1.0f );
        float32v noise = FS::Abs( this->GetSourceValue( mSource, seed, pos... ) );

        float32v sum = (noise * float32v( -2 ) + float32v( 1 )) * amp;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= Lerp( float32v( 1 ), float32v( 1 ) - noise, weightedStrength );
            amp *= gain;

            noise = FS::Abs( this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) );
            sum += (noise * float32v( -2 ) + float32v( 1 )) * amp;
        }

        return sum;
    }
};
