#include "FastSIMD/InlInclude.h"

#include "Fractal.h"

template<typename FS, typename T>
class FS_T<FastNoise::Fractal<T>, FS> : public virtual FastNoise::Fractal<T>, public FS_T<FastNoise::Modifier<T>, FS>
{

};

template<typename FS>
class FS_T<FastNoise::FractalFBm, FS> : public virtual FastNoise::FractalFBm, public FS_T<FastNoise::Fractal<>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos )
    {
        float32v sum = this->mSource[0]->Gen( seed, pos... );
        float32v amp = float32v( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= float32v( mGain );
            sum += this->mSource[0]->Gen( seed, (pos *= float32v( mLacunarity ))... ) * amp;
        }

        return sum * float32v( mFractalBounding );
    }
};

template<typename FS>
class FS_T<FastNoise::FractalBillow, FS> : public virtual FastNoise::FractalBillow, public FS_T<FastNoise::Fractal<>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos )
    {
        float32v sum = FS_Abs_f32( this->mSource[0]->Gen( seed, pos... ) ) * float32v( 2 ) - float32v( 1 );
        float32v amp = float32v( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= float32v( mGain );
            sum += (FS_Abs_f32(this->mSource[0]->Gen( seed, (pos *= float32v( mLacunarity ))... ) ) * float32v( 2 ) - float32v( 1 )) * amp;
        }

        return sum * float32v( mFractalBounding );
    }
};

template<typename FS>
class FS_T<FastNoise::FractalRidged, FS> : public virtual FastNoise::FractalRidged, public FS_T<FastNoise::Fractal<>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT(int32v seed, P... pos)
    {
        float32v sum = float32v( 1 ) - FS_Abs_f32( this->mSource[0]->Gen( seed, pos... ) );
        float32v amp = float32v( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= float32v( mGain );
            sum -= (float32v( 1 ) - FS_Abs_f32( this->mSource[0]->Gen( seed, (pos *= float32v( mLacunarity ))... ) )) * amp;
        }

        return sum;
    }
};

template<typename FS>
class FS_T<FastNoise::FractalRidgedMulti, FS> : public virtual FastNoise::FractalRidgedMulti, public FS_T<FastNoise::Fractal<>, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos )
    {
        float32v offset = float32v( 1 );
        float32v sum = offset - FS_Abs_f32( this->mSource[0]->Gen( seed, pos... ) );
        //sum *= sum;
        float32v amp = sum;

        float weight = mWeightAmp;
        float totalWeight = 1.0f;

        for( int i = 1; i < mOctaves; i++ )
        {
            amp *= float32v( mGain * 6 );
            amp = FS_Min_f32( FS_Max_f32( amp, float32v( 0 ) ), float32v( 1 ) );

            seed -= int32v( -1 );
            float32v value = offset - FS_Abs_f32( this->mSource[0]->Gen( seed, (pos *= float32v( mLacunarity ) )... ));

            value *= amp;
            amp = value;
            sum += value * FS_Reciprocal_f32( float32v( weight ) );

            totalWeight += 1.0f / weight;
            weight *= mWeightAmp;
        }

        return sum * float32v( mWeightBounding ) - offset;
    }
};
