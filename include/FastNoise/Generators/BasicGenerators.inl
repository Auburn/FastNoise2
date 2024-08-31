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

template<FastSIMD::FeatureSet SIMD, typename PARENT>
class FastSIMD::DispatchClass<FastNoise::VariableRange<PARENT>, SIMD> : public virtual FastNoise::VariableRange<PARENT>, public FastSIMD::DispatchClass<PARENT, SIMD>
{
protected:
    FS_FORCEINLINE float32v ScaleOutput( float32v value, float nativeMin, float nativeMax ) const
    {
        return FS::FMulAdd( float32v( 1.0f / ( nativeMax - nativeMin ) ) * float32v( this->mRangeScale ), value - float32v( nativeMin ), float32v( this->mRangeMin ) );
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
class FastSIMD::DispatchClass<FastNoise::White, SIMD> final : public virtual FastNoise::White, public FastSIMD::DispatchClass<FastNoise::VariableRange<Generator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos = FS::Cast<float>( (FS::Cast<int32_t>( pos ) ^ (FS::Cast<int32_t>( pos ) >> 16)) * int32v( Primes::Lookup[idx++] ) )), ...);

        return this->ScaleOutput( GetValueCoord( seed, FS::Cast<int32_t>( pos )... ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Checkerboard, SIMD> final : public virtual FastNoise::Checkerboard, public FastSIMD::DispatchClass<FastNoise::VariableRange<ScalableGenerator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( pos... );

        int32v value = (FS::Convert<int32_t>( pos ) ^ ...);

        return this->ScaleOutput( FS::Cast<float>( (value & int32v( 1 )) << 30 ), 0, 2 );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::SineWave, SIMD> final : public virtual FastNoise::SineWave, public FastSIMD::DispatchClass<FastNoise::VariableRange<ScalableGenerator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( pos... );

        return this->ScaleOutput( (FS::Sin( pos ) * ...), -1, 1 );
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
        float32v r( 0 );

        ((r = FS::FMulAdd( pos + this->GetSourceValue( mOffset[offsetIdx++], seed, pos... ), float32v( mMultiplier[multiplierIdx++]), r )), ...);
        return r;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DistanceToPoint, SIMD> final : public virtual FastNoise::DistanceToPoint, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        [this, seed] ( P&... out, std::remove_reference_t<P>... pos )
        {
            size_t pointIdx = 0;
            ((out -= this->GetSourceValue( mPoint[pointIdx++], seed, pos... )), ...);

        }( pos..., pos... );

        return CalcDistance( mDistanceFunction, mMinkowskiP, seed, pos... );
    }
};
