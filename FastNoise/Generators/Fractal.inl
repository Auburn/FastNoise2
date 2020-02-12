#define FASTSIMD_INTELLISENSE
#include "Fractal.h"

template<typename F, FastSIMD::ELevel S>
template<typename... P>
typename FS_CLASS( FastNoise::FractalFBm )<F, S>::float32v
FS_CLASS( FastNoise::FractalFBm )<F, S>::GenT( int32v seed, P... pos )
{
    float32v sum = mSource->Gen( seed, pos... );
    float32v amp = float32v( 1 );

    for ( int i = 1; i < mOctaves; i++ )
    {
        seed -= int32v( -1 );
        amp *= float32v( mGain );
        sum += mSource->Gen( seed, (pos *= float32v( mLacunarity ))... ) * amp;
    }

    return sum * float32v( mFractalBounding );
}

template<typename F, FastSIMD::ELevel S>
template<typename... P>
typename FS_CLASS( FastNoise::FractalBillow )<F, S>::float32v
FS_CLASS( FastNoise::FractalBillow )<F, S>::GenT( int32v seed, P... pos )
{
    float32v sum = FS_Abs_f32( mSource->Gen( seed, pos... ) ) * float32v( 2 ) - float32v( 1 );
    float32v amp = float32v( 1 );

    for ( int i = 1; i < mOctaves; i++ )
    {
        seed -= int32v( -1 );
        amp *= float32v( mGain );
        sum += (FS_Abs_f32( mSource->Gen( seed, (pos *= float32v( mLacunarity ))... ) ) * float32v( 2 ) - float32v( 1 )) * amp;
    }

    return sum * float32v( mFractalBounding );
}

template<typename F, FastSIMD::ELevel S>
template<typename... P>
typename FS_CLASS( FastNoise::FractalRidged )<F, S>::float32v
FS_CLASS( FastNoise::FractalRidged )<F, S>::GenT( int32v seed, P... pos )
{
    float32v sum = float32v( 1 ) - FS_Abs_f32( mSource->Gen( seed, pos... ) );
    float32v amp = float32v( 1 );

    for ( int i = 1; i < mOctaves; i++ )
    {
        seed -= int32v( -1 );
        amp *= float32v( mGain );
        sum -= (float32v( 1 ) - FS_Abs_f32( mSource->Gen( seed, (pos *= float32v( mLacunarity ))... ) )) * amp;
    }

    return sum;
}

template<typename F, FastSIMD::ELevel S>
template<typename... P>
typename FS_CLASS( FastNoise::FractalRidgedMulti )<F, S>::float32v
FS_CLASS( FastNoise::FractalRidgedMulti )<F, S>::GenT( int32v seed, P... pos )
{
    float32v offset = float32v( 1 );
    float32v sum = offset - FS_Abs_f32( mSource->Gen( seed, pos... ) );
    //sum *= sum;
    float32v amp = sum;

    float weight = mWeightAmp;
    float totalWeight = 1.0f;

    for ( int i = 1; i < mOctaves; i++ )
    {
        amp *= float32v( mGain * 6 );
        amp = FS_Min_f32( FS_Max_f32( amp, float32v( 0 ) ), float32v( 1 ) );

        seed -= int32v( -1 );
        float32v value = (offset - FS_Abs_f32( mSource->Gen( seed, (pos *= float32v( mLacunarity ))... ) ));
    
        value *= amp;
        amp = value;
        sum += value * FS_Reciprocal_f32( float32v( weight ) );

        totalWeight += 1.0f / weight;
        weight *= mWeightAmp;
    }

    return sum * float32v( mWeightBounding ) - offset;
}