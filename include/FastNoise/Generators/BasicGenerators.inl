#include <cassert>
#include "FastSIMD/InlInclude.h"

#include "BasicGenerators.h"

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<typename FS>
class FS_T<FastNoise::Checkerboard, FS> : public virtual FastNoise::Checkerboard, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v multiplier = FS_Reciprocal_f32( float32v( mSize ) );

        int32v value = (FS_Convertf32_i32( pos * multiplier ) ^ ...);

        return FS_BitwiseXor_f32( float32v( 1.0f ), FS_Casti32_f32( value << 31 ) );
    }
};

template<typename FS>
class FS_T<FastNoise::SineWave, FS> : public virtual FastNoise::SineWave, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v multiplier = FS_Reciprocal_f32( float32v( mScale ) );

        return (FS_Sin_f32( pos * multiplier ) * ...);
    }
};

template<typename FS>
class FS_T<FastNoise::PositionOutput, FS> : public virtual FastNoise::PositionOutput, public FS_T<FastNoise::Generator, FS>
{
public:
    //FASTNOISE_IMPL_GEN_T;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        float32v out = (x + float32v( mOffsetX )) * float32v( mMultiplierX );
        out += (y + float32v( mOffsetY )) * float32v( mMultiplierY );

        return out;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        float32v out = (x + float32v( mOffsetX )) * float32v( mMultiplierX );
        out += (y + float32v( mOffsetY )) * float32v( mMultiplierY );
        out += (y + float32v( mOffsetZ )) * float32v( mMultiplierZ );

        return out;
    }
};
