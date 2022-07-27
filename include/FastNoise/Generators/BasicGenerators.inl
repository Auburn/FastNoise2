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
        ((pos = FS::Cast<float>( (FS::Cast<int32_t>( pos ) ^ (FS::Cast<int32_t>( pos ) >> 16)) * int32v( FnPrimes::Lookup[idx++] ) )), ...);

        return FnUtils::GetValueCoord( seed, FS::Cast<int32_t>( pos )... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Checkerboard, SIMD> : public virtual FastNoise::Checkerboard, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        int32v value = (FS::Convert<int32_t>( pos * float32v( mSizeInv ) ) ^ ...);

        return float32v( 1.0f ) ^ FS::Cast<float>( value << 31 );
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
        size_t offsetIdx = 0;
        size_t multiplierIdx = 0;

        (((pos += float32v( mOffset[offsetIdx++] )) *= float32v( mMultiplier[multiplierIdx++] )), ...);
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
