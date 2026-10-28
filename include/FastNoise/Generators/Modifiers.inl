#include "Modifiers.h"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainScale, SIMD> final : public virtual DomainScale, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed, (pos * float32v( mScale ))... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainOffset, SIMD> final : public virtual DomainOffset, public DispatchClass<Generator, SIMD>
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
        // With constant offsets the positions are only used by the source call, see CombineSourceValues
        if( ( !mOffset[I].simdGeneratorPtr && ... ) )
        {
            ((pos += float32v( mOffset[I].constant )), ...);

            return this->GetSourceValue( mSource, seed, pos... );
        }

        // Braced init evaluates left to right; expanding into the call is right to left on the MS ABI, which changes fast-math output
        float32v offsetPos[] = { ( pos + this->GetSourceValue( mOffset[I], seed, pos... ) )... };

        return this->GetSourceValue( mSource, seed, offsetPos[I]... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainRotate, SIMD> final : public virtual DomainRotate, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        if( mPitchSin == 0.0f && mRollSin == 0.0f )
        {
            return this->GetSourceValue( mSource, seed,
                FS::FNMulAdd( y, float32v( mYawSin ), x * float32v( mYawCos ) ),
                FS::FMulAdd( x, float32v( mYawSin ), y * float32v( mYawCos ) ) );
        }

        return Gen( seed, x, y, float32v( 0 ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        return this->GetSourceValue( mSource, seed,
            FS::FMulAdd( x, float32v( mXa ), FS::FMulAdd( y, float32v( mXb ), z * float32v( mXc ) ) ),
            FS::FMulAdd( x, float32v( mYa ), FS::FMulAdd( y, float32v( mYb ), z * float32v( mYc ) ) ),
            FS::FMulAdd( x, float32v( mZa ), FS::FMulAdd( y, float32v( mZb ), z * float32v( mZc ) ) ) );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        // No rotation for 4D yet
        return this->GetSourceValue( mSource, seed, x, y, z, w );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<SeedOffset, SIMD> final : public virtual SeedOffset, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed + int32v( mOffset ), pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Remap, SIMD> final : public virtual Remap, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        // With constant ranges the positions are only used by the source call, see CombineSourceValues
        if( !mFromMin.simdGeneratorPtr && !mFromMax.simdGeneratorPtr && !mToMin.simdGeneratorPtr && !mToMax.simdGeneratorPtr )
        {
            float fromMin = mFromMin.constant;
            float fromMax = mFromMax.constant;
            float toMin = mToMin.constant;
            float toMax = mToMax.constant;

            return Remap( this->GetSourceValue( mSource, seed, pos... ), float32v( fromMin ), float32v( fromMax ), float32v( toMin ), float32v( toMax ) );
        }

        float32v source = this->GetSourceValue( mSource, seed, pos... );

        float32v fromMin = this->GetSourceValue( mFromMin, seed, pos... );
        float32v fromMax = this->GetSourceValue( mFromMax, seed, pos... );
        float32v toMin = this->GetSourceValue( mToMin, seed, pos... );
        float32v toMax = this->GetSourceValue( mToMax, seed, pos... );

        return Remap( source, fromMin, fromMax, toMin, toMax );
    }

    FS_FORCEINLINE float32v Remap( float32v source, float32v fromMin, float32v fromMax, float32v toMin, float32v toMax ) const
    {
        float32v result = toMin + ( ( source - fromMin ) / ( fromMax - fromMin ) * ( toMax - toMin ) );

        if( mClampOutput == Boolean::True )
        {
            result = FS::Max( result, toMin );
            result = FS::Min( result, toMax );
        }

        return result;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<ConvertRGBA8, SIMD> final : public virtual ConvertRGBA8, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
        
        source = FS::Min( source, float32v( mMax ));
        source = FS::Max( source, float32v( mMin ));
        source -= float32v( mMin );

        source *= float32v( 255.0f / (mMax - mMin) );

        int32v byteVal = FS::Convert<std::int32_t>( source );

        int32v output = int32v( 255 << 24 );
        output |= byteVal;
        output |= byteVal << 8;
        output |= byteVal << 16;

        return FS::Cast<float>( output );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Terrace, SIMD> final : public virtual Terrace, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        // With constant smoothness the positions are only used by the source call, see CombineSourceValues
        if( mSmoothness.simdGeneratorPtr )
        {
            return GenTerrace<true>( seed, pos... );
        }
        return GenTerrace<false>( seed, pos... );
    }

    template<bool DYNAMIC_SMOOTHNESS, typename... P>
    FS_FORCEINLINE float32v GenTerrace( int32v seed, P... pos ) const
    {
        bool constantSmoothness = !DYNAMIC_SMOOTHNESS && mSmoothness.constant != 0.0f; // Read before the source call, see CombineSourceValues

        float32v source = this->GetSourceValue( mSource, seed, pos... );

        if constexpr( DYNAMIC_SMOOTHNESS )
        {
            return TerraceValue<true>( source, false, this->GetSourceValue( mSmoothness, seed, pos... ) );
        }
        else
        {
            return TerraceValue<false>( source, constantSmoothness, float32v( 0 ) );
        }
    }

    template<bool DYNAMIC_SMOOTHNESS>
    FS_FORCEINLINE float32v TerraceValue( float32v source, bool constantSmoothness, float32v smoothness ) const
    {
        source *= float32v( mStepCount );
        float32v rounded = FS::Round( source );

        if constexpr( DYNAMIC_SMOOTHNESS )
        {
            // Dynamic smoothness from generator
            mask32v smoothnessMask = smoothness != float32v( 0.0f );

            if( FS::AnyMask( smoothnessMask ) )
            {
                float32v diff = rounded - source;
                mask32v diffSign = diff < float32v( 0 );

                diff = FS::Abs( diff );
                diff = float32v( 0.5f ) - diff;

                float32v smoothnessRecip = float32v( 1.0f ) + FS::Reciprocal( smoothness );
                diff *= smoothnessRecip;
                diff = FS::Min( diff, float32v( 0.5f ) );
                diff = FS::Select( diffSign, float32v( 0.5f ) - diff, diff - float32v( 0.5f ) );

                rounded = FS::Select( smoothnessMask, rounded + diff, rounded );
            }
        }
        else if( constantSmoothness )
        {
            // Constant smoothness, use precomputed reciprocal
            float32v diff = rounded - source;
            mask32v diffSign = diff < float32v( 0 );

            diff = FS::Abs( diff );
            diff = float32v( 0.5f ) - diff;

            diff *= float32v( mSmoothnessRecip );
            diff = FS::Min( diff, float32v( 0.5f ) );
            diff = FS::Select( diffSign, float32v( 0.5f ) - diff, diff - float32v( 0.5f ) );

            rounded += diff;
        }

        return rounded * float32v( mStepCountRecip );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<PingPong, SIMD> final : public virtual PingPong, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    static float32v PingPong( float32v t )
    {
        t -= FS::Floor( t * float32v( 0.5f ) ) * float32v( 2 );
        return FS::Select( t < float32v( 1 ), t, float32v( 2 ) - t );
    }

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v pingPongStrength = this->GetSourceValue( mPingPongStrength, seed, pos... );
        return PingPong( this->GetSourceValue( mSource, seed, pos... ) * pingPongStrength );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainAxisScale, SIMD> final : public virtual DomainAxisScale, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos *= float32v( mScale[idx++] )), ...);

        return this->GetSourceValue( mSource, seed, pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<AddDimension, SIMD> final : public virtual AddDimension, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        if constexpr( sizeof...(P) == (size_t)FastNoise::Dim::Count )
        {
            return this->GetSourceValue( mSource, seed, pos... );
        }
        else
        {
            return this->GetSourceValue( mSource, seed, pos..., this->GetSourceValue( mNewDimensionPosition, seed, pos... ) );
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<RemoveDimension, SIMD> final : public virtual RemoveDimension, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        return this->GetSourceValue( mSource, seed, x, y );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        switch( mRemoveDimension )
        {
        case FastNoise::Dim::X:
            return this->GetSourceValue( mSource, seed, y, z );
        case FastNoise::Dim::Y:
            return this->GetSourceValue( mSource, seed, x, z );
        case FastNoise::Dim::Z:
            return this->GetSourceValue( mSource, seed, x, y );
        default:
            return this->GetSourceValue( mSource, seed, x, y, z );
        }
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        switch( mRemoveDimension )
        {
        case FastNoise::Dim::X:
            return this->GetSourceValue( mSource, seed, y, z, w );
        case FastNoise::Dim::Y:
            return this->GetSourceValue( mSource, seed, x, z, w );
        case FastNoise::Dim::Z:
            return this->GetSourceValue( mSource, seed, x, y, w );
        case FastNoise::Dim::W:
            return this->GetSourceValue( mSource, seed, x, y, z );
        default:
            return this->GetSourceValue( mSource, seed, x, y, z, w );
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<GeneratorCache, SIMD> final : public virtual GeneratorCache, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        static constexpr size_t kCacheAlign = 64;
        static constexpr std::uint32_t kSlotCount = 4;
        static constexpr size_t kCacheFloats = ( sizeof...(P) + 1 + kSlotCount ) * int32v::ElementCount;

        // One key (seed + pos) with several value slots, each tagged with the source generator it holds,
        // so different cache nodes evaluated at the same positions (e.g. two cached nodes both used twice) don't evict each other.
        // A slot is tagged before its source is evaluated. A nested cache can't have the same source so never reads it early,
        // if a nested cache takes the slot or replaces the key the value isn't stored (or lands beyond slotsUsed, where it is ignored)
        struct ThreadCache
        {
            const void* generators[kSlotCount];
            std::uint32_t slotsUsed;
            float storage[kCacheFloats + kCacheAlign / sizeof( float )];
        };
        thread_local static ThreadCache Cache;

        // TLS is not always aligned (compiler bug), need to avoid using SIMD types
        // Manually align so cached vectors don't split cache lines (breaks store forwarding on the hit path)
        float* cache = reinterpret_cast<float*>( ( reinterpret_cast<uintptr_t>( Cache.storage ) + ( kCacheAlign - 1 ) ) & ~uintptr_t( kCacheAlign - 1 ) );
        std::int32_t* cachedSeed = reinterpret_cast<std::int32_t*>( cache );
        float* cachedPos = cache + int32v::ElementCount;
        float* cachedValues = cache + ( sizeof...(P) + 1 ) * int32v::ElementCount;

        const void* source = mSource.simdGeneratorPtr;
        const float32v arrayPos[] = { pos... };

        bool isSameKey = !FS::AnyMask( seed != FS::Load<int32v>( cachedSeed ) );

        for( size_t i = 0; i < sizeof...( P ); i++ )
        {
            isSameKey &= !FS::AnyMask( arrayPos[i] != FS::Load<float32v>( cachedPos + i * int32v::ElementCount ) );
        }

        std::uint32_t slot = 0;

        if( isSameKey )
        {
            // Slot 0 belongs to the cache that stored the key, the only one used unless several cache nodes share positions
            if( Cache.generators[0] == source )
            {
                return FS::Load<float32v>( cachedValues );
            }

            for( std::uint32_t i = 1; i < Cache.slotsUsed; i++ )
            {
                if( Cache.generators[i] == source )
                {
                    return FS::Load<float32v>( cachedValues + i * int32v::ElementCount );
                }
            }

            // Use the next slot, replace the last one if all are in use
            slot = std::min( Cache.slotsUsed, kSlotCount - 1 );
        }
        else
        {
            // Store the key before evaluating the source so seed/pos don't need preserving across the call
            FS::Store( cachedSeed, seed );

            for( size_t i = 0; i < sizeof...(P); i++ )
            {
                FS::Store( cachedPos + i * int32v::ElementCount, arrayPos[i] );
            }
        }

        Cache.slotsUsed = slot + 1;
        Cache.generators[slot] = source;

        float32v value = this->GetSourceValue( mSource, seed, pos... );

        if( Cache.generators[slot] == source )
        {
            FS::Store( cachedValues + slot * int32v::ElementCount, value );
        }

        return value;
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<SignedSquareRoot, SIMD> final : public virtual SignedSquareRoot, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v value = this->GetSourceValue( mSource, seed, pos... );
        
        return FastLengthSqrt( FS::Abs( value ) ) | FS::SignBit( value );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Abs, SIMD> final : public virtual Abs, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v value = this->GetSourceValue( mSource, seed, pos... );
        
        return FS::Abs( value );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainRotatePlane, SIMD> final : public virtual DomainRotatePlane, public DispatchClass<Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    FS_FORCEINLINE void FS_VECTORCALL RotateCoords( PlaneRotationType rotationType, float32v& x, float32v& y, float32v& z ) const
    {
        float32v newX = x;
        float32v newY = y;
        float32v newZ = z;

        switch( rotationType )
        {
        case PlaneRotationType::ImproveXYPlanes:
        {
            float32v xy = x + y;
            float32v s2 = xy * float32v( -0.211324865405187f );
            newZ = z * float32v( 0.577350269189626f );
            newX = x + s2 - newZ;
            newY = y + s2 - newZ;
            newZ = newZ + xy * float32v( 0.577350269189626f );
        }
            break;

        case PlaneRotationType::ImproveXZPlanes:
        {
            float32v xz = x + z;
            float32v s2 = xz * float32v( -0.211324865405187f );
            newY = y * float32v( 0.577350269189626f );
            newX = x + s2 - newY;
            newZ = z + s2 - newY;
            newY = newY + xz * float32v( 0.577350269189626f );

        }
            break;
        }
        x = newX;
        y = newY;
        z = newZ;
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        float32v z = 0;
        RotateCoords( PlaneRotationType::ImproveXYPlanes, x, y, z );

        return this->GetSourceValue( mSource, seed, x, y, z );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        RotateCoords( mRotationType, x, y, z );
        return this->GetSourceValue( mSource, seed, x, y, z );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        RotateCoords( mRotationType, x, y, z );
        return this->GetSourceValue( mSource, seed, x, y, z, w );
    }
};
