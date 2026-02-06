#include "BasicGenerators.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD, typename PARENT>
class FastSIMD::DispatchClass<Seeded<PARENT>, SIMD> : public virtual Seeded<PARENT>, public DispatchClass<PARENT, SIMD>
{

};


template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<ScalableGenerator, SIMD> : public virtual ScalableGenerator, public DispatchClass<Generator, SIMD>
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
class FastSIMD::DispatchClass<VariableRange<PARENT>, SIMD> : public virtual VariableRange<PARENT>, public DispatchClass<PARENT, SIMD>
{
protected:
    FS_FORCEINLINE float32v ScaleOutput( float32v value, float nativeMin, float nativeMax ) const
    {
        return FS::FMulAdd( float32v( 1.0f / ( nativeMax - nativeMin ) ) * float32v( this->mRangeScale ), value - float32v( nativeMin ), float32v( this->mRangeMin ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Constant, SIMD> final : public virtual Constant, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<White, SIMD> final : public virtual White, public DispatchClass<VariableRange<Seeded<Generator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        seed += int32v( mSeedOffset );
        size_t idx = 0;
        ((pos = FS::Cast<float>( (FS::Cast<int32_t>( pos ) ^ (FS::Cast<int32_t>( pos ) >> 16)) * int32v( Primes::Lookup[idx++] ) )), ...);

        return this->ScaleOutput( GetValueCoord( seed, FS::Cast<int32_t>( pos )... ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Checkerboard, SIMD> final : public virtual Checkerboard, public DispatchClass<VariableRange<ScalableGenerator>, SIMD>
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
class FastSIMD::DispatchClass<SineWave, SIMD> final : public virtual SineWave, public DispatchClass<VariableRange<ScalableGenerator>, SIMD>
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
class FastSIMD::DispatchClass<Gradient, SIMD> final : public virtual Gradient, public DispatchClass<Generator, SIMD>
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
class FastSIMD::DispatchClass<DistanceToPoint, SIMD> final : public virtual DistanceToPoint, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        [self = this, seed] ( P&... out, std::remove_reference_t<P>... pos )
        {
            size_t pointIdx = 0;
            ((out -= self->GetSourceValue( self->mPoint[pointIdx++], seed, pos... )), ...);

        }( pos..., pos... );

        return CalcDistance( mDistanceFunction, mMinkowskiP, seed, pos... );
    }
};
