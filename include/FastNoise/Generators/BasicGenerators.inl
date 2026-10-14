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
    // Takes the final generator so mFrequency is read at a static offset instead of through the virtual base table
    template<typename SELF, typename... P>
    static FS_FORCEINLINE void ScalePositions( const SELF* self, P&... pos )
    {
        float32v vFrequency( self->mFrequency );
        ( (pos *= vFrequency), ... );
    }
};

template<FastSIMD::FeatureSet SIMD, typename PARENT>
class FastSIMD::DispatchClass<VariableRange<PARENT>, SIMD> : public virtual VariableRange<PARENT>, public DispatchClass<PARENT, SIMD>
{
protected:
    // Takes the final generator so the range is read at static offsets instead of through the virtual base table
    template<typename SELF>
    static FS_FORCEINLINE float32v ScaleOutput( const SELF* self, float32v value, float nativeMin, float nativeMax )
    {
        // Scalar multiply: as a vector multiply clang folds the field into a wide load of the object, which measured much slower
        return FS::FMulAdd( float32v( self->mRangeScale * ( 1.0f / ( nativeMax - nativeMin ) ) ), value - float32v( nativeMin ), float32v( self->mRangeMin ) );
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

        return this->ScaleOutput( this, GetValueCoord( seed, FS::Cast<int32_t>( pos )... ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Checkerboard, SIMD> final : public virtual Checkerboard, public DispatchClass<VariableRange<ScalableGenerator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( this, pos... );

        int32v value = (FS::Convert<int32_t>( pos ) ^ ...);

        return this->ScaleOutput( this, FS::Cast<float>( (value & int32v( 1 )) << 30 ), 0, 2 );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<SineWave, SIMD> final : public virtual SineWave, public DispatchClass<VariableRange<ScalableGenerator>, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        this->ScalePositions( this, pos... );

        return this->ScaleOutput( this, (FS::Sin( pos ) * ...), -1, 1 );
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

        bool hasZeroMultiplier = false;
        for( size_t i = 0; i < sizeof...( P ); i++ )
        {
            hasZeroMultiplier |= mMultiplier[i] == 0.0f;
        }

        if( !hasZeroMultiplier )
        {
            ((r = FS::FMulAdd( pos + this->GetSourceValue( mOffset[offsetIdx++], seed, pos... ), float32v( mMultiplier[multiplierIdx++]), r )), ...);
            return r;
        }

        // ( pos + offset ) * 0 doesn't change r, skip it and avoid evaluating the offset source
        ((r = mMultiplier[offsetIdx] != 0.0f ? FS::FMulAdd( pos + this->GetSourceValue( mOffset[offsetIdx], seed, pos... ), float32v( mMultiplier[offsetIdx] ), r ) : r, offsetIdx++), ...);
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
        return GenT( std::index_sequence_for<P...>(), seed, pos... );
    }

    template<size_t... I, typename... P>
    FS_FORCEINLINE float32v GenT( std::index_sequence<I...>, int32v seed, P... pos ) const
    {
        // With constant point (and Minkowski P) values no source is called, so the positions don't need preserving across calls
        bool hasSource = mMinkowskiP.simdGeneratorPtr && mDistanceFunction == DistanceFunction::Minkowski;
        ((hasSource |= mPoint[I].simdGeneratorPtr != nullptr), ...);

        if( !hasSource )
        {
            // Subtract in order before the call, see the braced init below
            ((pos -= float32v( mPoint[I].constant )), ...);

            return CalcDistance( mDistanceFunction, float32v( mMinkowskiP.constant ), pos... );
        }

        float32v minkowskiP = GetMinkowskiP( mDistanceFunction, mMinkowskiP, seed, pos... );

        // Braced init evaluates left to right; expanding into the call is right to left on the MS ABI, which changes fast-math output
        float32v offsetPos[] = { ( pos - this->GetSourceValue( mPoint[I], seed, pos... ) )... };

        return CalcDistance( mDistanceFunction, minkowskiP, offsetPos[I]... );
    }
};
