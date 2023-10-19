#include "BasicGenerators.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD> : public virtual FastNoise::ScalableGenerator, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
protected:
    template<typename... P>
    FS_FORCEINLINE void ScalePositions( P&... pos ) const
    {
        float32v vFrequency( mFrequency );
        ( (pos *= vFrequency), ... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Constant, SIMD> final : public virtual FastNoise::Constant, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::White, SIMD> final : public virtual FastNoise::White, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos = FS::Cast<float>( (FS::Cast<int32_t>( pos ) ^ (FS::Cast<int32_t>( pos ) >> 16)) * int32v( Primes::Lookup[idx++] ) )), ...);

        return GetValueCoord( seed, FS::Cast<int32_t>( pos )... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Checkerboard, SIMD> final : public virtual FastNoise::Checkerboard, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( pos... );

        int32v value = (FS::Convert<int32_t>( pos ) ^ ...);

        return float32v( 1.0f ) ^ FS::Cast<float>( value << 31 );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::SineWave, SIMD> final : public virtual FastNoise::SineWave, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( pos... );

        return (FS::Sin( pos ) * ...);
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::PositionOutput, SIMD> final : public virtual FastNoise::PositionOutput, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
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
class FastSIMD::DispatchClass<FastNoise::DistanceToPoint, SIMD> final : public virtual FastNoise::DistanceToPoint, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t pointIdx = 0;

        ((pos -= float32v( mPoint[pointIdx++] )), ...);
        return CalcDistance( mDistanceFunction, pos... );
    }
};
