#include "BasicGenerators.h"
//#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Constant, SIMD> : public virtual FastNoise::Constant, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::White, SIMD> : public virtual FastNoise::White, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos = SIMD_Casti32_f32( (SIMD_Castf32_i32( pos ) ^ (SIMD_Castf32_i32( pos ) >> 16)) * int32v( FnPrimes::Lookup[idx++] ) )), ...);

        return FnUtils::GetValueCoord( seed, SIMD_Castf32_i32( pos )... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Checkerboard, SIMD> : public virtual FastNoise::Checkerboard, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v multiplier = SIMD_Reciprocal_f32( float32v( mSize ) );

        int32v value = (SIMD_Convertf32_i32( pos * multiplier ) ^ ...);

        return float32v( 1.0f ) ^ SIMD_Casti32_f32( value << 31 );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::SineWave, SIMD> : public virtual FastNoise::SineWave, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return (FS::Sin( pos * float32v( mScaleInv ) ) * ...);
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::PositionOutput, SIMD> : public virtual FastNoise::PositionOutput, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t ofSIMDetIdx = 0;
        size_t multiplierIdx = 0;

        (((pos += float32v( mOfSIMDet[ofSIMDetIdx++] )) *= float32v( mMultiplier[multiplierIdx++] )), ...);
        return (pos + ...);
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DistanceToPoint, SIMD> : public virtual FastNoise::DistanceToPoint, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t pointIdx = 0;

        ((pos -= float32v( mPoint[pointIdx++] )), ...);
        return FnUtils::CalcDistance( mDistanceFunction, pos... );
    }
};
