#include "Modifiers.h"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainScale, SIMD> final : public virtual FastNoise::DomainScale, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed, (pos * float32v( mScale ))... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainOffset, SIMD> final : public virtual FastNoise::DomainOffset, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return [this, seed]( std::remove_reference_t<P>... sourcePos, std::remove_reference_t<P>... offset )
        {
            size_t idx = 0;
            ((offset += this->GetSourceValue( mOffset[idx++], seed, sourcePos... )), ...);

            return this->GetSourceValue( mSource, seed, offset... );
        } (pos..., pos...);
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainRotate, SIMD> final : public virtual FastNoise::DomainRotate, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        if( mPitchSin == 0.0f && mRollSin == 0.0f )
        {
            return this->GetSourceValue( mSource, seed,
                FS::FNMulAdd( y, float32v( mYawSin ), x * float32v( mYawCos ) ),
                FS::FMulAdd( x, float32v( mYawSin ), y * float32v( mYawCos ) ) );
        }

        return Gen( seed, x, y, float32v( 0 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        return this->GetSourceValue( mSource, seed,
            FS::FMulAdd( x, float32v( mXa ), FS::FMulAdd( y, float32v( mXb ), z * float32v( mXc ) ) ),
            FS::FMulAdd( x, float32v( mYa ), FS::FMulAdd( y, float32v( mYb ), z * float32v( mYc ) ) ),
            FS::FMulAdd( x, float32v( mZa ), FS::FMulAdd( y, float32v( mZb ), z * float32v( mZc ) ) ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        // No rotation for 4D yet
        return this->GetSourceValue( mSource, seed, x, y, z, w );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::SeedOffset, SIMD> final : public virtual FastNoise::SeedOffset, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed + int32v( mOffset ), pos... );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Remap, SIMD> final : public virtual FastNoise::Remap, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
            
        return float32v( mToMin ) + (( source - float32v( mFromMin ) ) / float32v( mFromMax - mFromMin ) * float32v( mToMax - mToMin ));
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::ConvertRGBA8, SIMD> final : public virtual FastNoise::ConvertRGBA8, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
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
class FastSIMD::DispatchClass<FastNoise::Terrace, SIMD> final : public virtual FastNoise::Terrace, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );

        source *= float32v( mMultiplier );
        float32v rounded = FS::Round( source );

        if( mSmoothness != 0.0f )
        {
            float32v diff = rounded - source;
            mask32v diffSign = diff < float32v( 0 );

            diff = FS::Abs( diff );
            diff = float32v( 0.5f ) - diff;

            diff *= float32v( mSmoothnessRecip );
            diff = FS::Min( diff, float32v( 0.5f ) );
            diff = FS::Select( diffSign, float32v( 0.5f ) - diff, diff - float32v( 0.5f ) );

            rounded += diff;
        }

        return rounded * float32v( mMultiplierRecip );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainAxisScale, SIMD> final : public virtual FastNoise::DomainAxisScale, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
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
class FastSIMD::DispatchClass<FastNoise::AddDimension, SIMD> final : public virtual FastNoise::AddDimension, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
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
class FastSIMD::DispatchClass<FastNoise::RemoveDimension, SIMD> final : public virtual FastNoise::RemoveDimension, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        return this->GetSourceValue( mSource, seed, x, y );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
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

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
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
class FastSIMD::DispatchClass<FastNoise::GeneratorCache, SIMD> final : public virtual FastNoise::GeneratorCache, public FastSIMD::DispatchClass<FastNoise::Generator, SIMD>
{
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_FORCEINLINE float32v GenT( int32v seed, P... pos ) const
    {
        thread_local static const void* CachedGenerator = nullptr;
        thread_local static std::int32_t CachedSeed[int32v::ElementCount];
        thread_local static float CachedPos[sizeof...(P)][int32v::ElementCount];
        thread_local static float CachedValue[int32v::ElementCount];
        // TLS is not always aligned (compiler bug), need to avoid using SIMD types
        
        const float32v arrayPos[] = { pos... };

        bool isSame = (CachedGenerator == mSource.simdGeneratorPtr);
        isSame &= !FS::AnyMask( seed != FS::Load<int32v>( CachedSeed ) );

        for( size_t i = 0; i < sizeof...( P ); i++ )
        {
            isSame &= !FS::AnyMask( arrayPos[i] != FS::Load<float32v>( CachedPos[i] ) );
        }

        if( !isSame )
        {
            CachedGenerator = mSource.simdGeneratorPtr;

            float32v value = this->GetSourceValue( mSource, seed, pos... );

            FS::Store( CachedValue, value );
            FS::Store( CachedSeed, seed );

            for( size_t i = 0; i < sizeof...(P); i++ )
            {
                FS::Store( CachedPos[i], arrayPos[i] );
            }

            return value;
        }

        return FS::Load<float32v>( CachedValue );
    }
};
