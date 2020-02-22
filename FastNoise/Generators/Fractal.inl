#define FASTSIMD_INTELLISENSE
#include "Fractal.h"

#include "FastSIMD/FunctionList.h"

namespace FastNoise
{
    template<typename FS>
    class Fractal_FS : public virtual Fractal, public Modifier_FS<FS, 1>
    {
        FASTSIMD_TYPEDEF;

    };

    template<typename FS>
    class FractalFBm_FS : public virtual FractalFBm, public Fractal_FS<FS>
    {
        FASTSIMD_TYPEDEF;

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
    class FractalBillow_FS : public virtual FractalBillow, public Fractal_FS<FS>
    {
        FASTSIMD_TYPEDEF;

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
    class FractalRidged_FS : public virtual FractalRidged, public Fractal_FS<FS>
    {
        FASTSIMD_TYPEDEF;

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
    class FractalRidgedMulti_FS : public virtual FractalRidgedMulti, public Fractal_FS<FS>
    {
        FASTSIMD_TYPEDEF;

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
}