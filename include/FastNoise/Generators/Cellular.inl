#include <cfloat>
#include <array>
#include <type_traits>

#include "Cellular.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD, typename PARENT>
class FastSIMD::DispatchClass<Cellular<PARENT>, SIMD> : public virtual Cellular<PARENT>, public DispatchClass<PARENT, SIMD>
{
protected:
    static constexpr float kJitter2D = 0.437016f;
    static constexpr float kJitter3D = 0.396144f;
    static constexpr float kJitter4D = 0.366025f;
    static constexpr float kJitterIdx23 = 0.190983f;

    template<DistanceFunction DIST_FUNC>
    using DistanceFunctionConstant = std::integral_constant<DistanceFunction, DIST_FUNC>;

    template<int COUNT>
    using CountConstant = std::integral_constant<int, COUNT>;

    // Picks the GenT variant by calling func( specialised, distanceFunction, count ) with compile time constants
    // Common settings get a cell loop specialised on distance function and number of tracked cells (count: 1, 2 or MAX_COUNT)
    // Everything else (size jitter, Minkowski, other counts) uses the generic variant: specialised = false
    // SQRT_EUCLIDEAN: Euclidean compares sqrt distances, otherwise it is equivalent to EuclideanSquared
    template<int MAX_COUNT, bool SQRT_EUCLIDEAN, typename FUNC>
    FS_FORCEINLINE auto SelectGenT( int count, FUNC&& func ) const
    {
        if( !this->mSizeJitter.simdGeneratorPtr && this->mSizeJitter.constant == 0.0f &&
            ( count == 1 || count == 2 || count == MAX_COUNT ) )
        {
            switch( this->mDistanceFunction )
            {
            case DistanceFunction::Euclidean:
                if constexpr( SQRT_EUCLIDEAN )
                {
                    return SelectCount<DistanceFunction::Euclidean, MAX_COUNT>( count, func );
                }
            case DistanceFunction::EuclideanSquared:
                return SelectCount<DistanceFunction::EuclideanSquared, MAX_COUNT>( count, func );
            case DistanceFunction::Manhattan:
                return SelectCount<DistanceFunction::Manhattan, MAX_COUNT>( count, func );
            case DistanceFunction::Hybrid:
                return SelectCount<DistanceFunction::Hybrid, MAX_COUNT>( count, func );
            case DistanceFunction::MaxAxis:
                return SelectCount<DistanceFunction::MaxAxis, MAX_COUNT>( count, func );
            default: // Minkowski, Pow() dominates
                break;
            }
        }

        return func( std::false_type{}, DistanceFunctionConstant<DistanceFunction::EuclideanSquared>{}, CountConstant<MAX_COUNT>{} );
    }

    template<DistanceFunction DIST_FUNC, int MAX_COUNT, typename FUNC>
    static FS_FORCEINLINE auto SelectCount( int count, FUNC& func )
    {
        constexpr DistanceFunctionConstant<DIST_FUNC> distFunc;

        if constexpr( MAX_COUNT > 2 )
        {
            if( count == 1 )
            {
                return func( std::true_type{}, distFunc, CountConstant<1>{} );
            }
            if( count == 2 )
            {
                return func( std::true_type{}, distFunc, CountConstant<2>{} );
            }
        }
        return func( std::true_type{}, distFunc, CountConstant<MAX_COUNT>{} );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularValue, SIMD> final : public virtual CellularValue, public DispatchClass<Cellular<>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, true>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcf );
                yd = FS::FMulAdd( yd, invMag, ycf );

                int32v newCellValueHash = hash;
                float32v newDistance = CalcDistance<true>( distFunc, minkowskiP, xd, yd );
                if( sizeJitterActive )
                {
                    float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
                    newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                }

                for( int i = 0; ; i++ )
                {
                    mask32v closer = newDistance < distance[i];

                    float32v localDistance = distance[i];
                    int32v localCellValueHash = valueHash[i];

                    distance[i] = FS::Select( closer, newDistance, distance[i] );
                    valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                    if( i >= ( SPECIALISED ? COUNT - 1 : mValueIndex ) )
                    {
                        break;
                    }

                    newDistance = FS::Select( closer, localDistance, newDistance );
                    newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
                }

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, false>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y, z );
        
        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        
        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
    
        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );
                
                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    xd = FS::FMulAdd( xd, invMag, xcf );
                    yd = FS::FMulAdd( yd, invMag, ycf );
                    zd = FS::FMulAdd( zd, invMag, zcf );

                    int32v newCellValueHash = hash;
                    float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
                    if( sizeJitterActive )
                    {
                        float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                        newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                    }
                
                    for( int i = 0; ; i++ )
                    {
                        mask32v closer = newDistance < distance[i];

                        float32v localDistance = distance[i];
                        int32v localCellValueHash = valueHash[i];

                        distance[i] = FS::Select( closer, newDistance, distance[i] );
                        valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                        if( i >= ( SPECIALISED ? COUNT - 1 : mValueIndex ) )
                        {
                            break;
                        }

                        newDistance = FS::Select( closer, localDistance, newDistance );
                        newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
                    }
            
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, false>( mValueIndex + 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z , float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        std::array<int32v, kMaxDistanceCount> valueHash;
        std::array<float32v, kMaxDistanceCount> distance;
        
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y, z, w );
        
        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );
        
        float32v xcf = FS::Convert<float>( xc ) - x;
        float32v ycfBase = FS::Convert<float>( ycBase ) - y;
        float32v zcfBase = FS::Convert<float>( zcBase ) - z;
        float32v wcfBase = FS::Convert<float>( wcBase ) - w;
    
        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        xd = FS::FMulAdd( xd, invMag, xcf );
                        yd = FS::FMulAdd( yd, invMag, ycf );
                        zd = FS::FMulAdd( zd, invMag, zcf );
                        wd = FS::FMulAdd( wd, invMag, wcf );

                        int32v newCellValueHash = hash;
                        float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
                        if( sizeJitterActive )
                        {
                            float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                            newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                        }

                        for( int i = 0; ; i++ )
                        {
                            mask32v closer = newDistance < distance[i];

                            float32v localDistance = distance[i];
                            int32v localCellValueHash = valueHash[i];

                            distance[i] = FS::Select( closer, newDistance, distance[i] );
                            valueHash[i] = FS::Select( closer, newCellValueHash, valueHash[i] );

                            if( i >= ( SPECIALISED ? COUNT - 1 : mValueIndex ) )
                            {
                                break;
                            }

                            newDistance = FS::Select( closer, localDistance, newDistance );
                            newCellValueHash = FS::Select( closer, localCellValueHash, newCellValueHash );
                        }

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->ScaleOutput( FS::Convert<float>( valueHash[mValueIndex] ), -kValueBounds, kValueBounds );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularDistance, SIMD> final : public virtual CellularDistance, public DispatchClass<Cellular<>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    // Number of closest distances to track, 3 is rounded up to use the specialised 4 variant
    int GetDistanceCount() const
    {
        int count = std::max( mDistanceIndex0, mReturnType == ReturnType::Index0 ? 0 : mDistanceIndex1 ) + 1;
        return count == 3 ? kMaxDistanceCount : count;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, false>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v xcfOffset = xcf - x;
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                xd = FS::FMulAdd( xd, invMag, xcfOffset );
                yd = FS::FMulAdd( yd, invMag, ycf - y );

                float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd );
                if( sizeJitterActive )
                {
                    float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
                    newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                }

                for( int i = COUNT - 1; i > 0; i-- )
                {
                    distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                }

                distance[0] = FS::Min( distance[0], newDistance );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance, 1 + this->kJitter2D );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, false>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y, z );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );
        float32v zcfBase = FS::Convert<float>( zcBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v xcfOffset = xcf - x;
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v ycfOffset = ycf - y;
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( (hash >> 11) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    xd = FS::FMulAdd( xd, invMag, xcfOffset );
                    yd = FS::FMulAdd( yd, invMag, ycfOffset );
                    zd = FS::FMulAdd( zd, invMag, zcf - z );

                    float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
                    if( sizeJitterActive )
                    {
                        float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                        newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                    }

                    for( int i = COUNT - 1; i > 0; i-- )
                    {
                        distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                    }

                    distance[0] = FS::Min( distance[0], newDistance );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance, 1 + this->kJitter3D );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<kMaxDistanceCount, false>( GetDistanceCount(), []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( kInfinity ) );

        this->ScalePositions( x, y, z, w );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );
        float32v zcfBase = FS::Convert<float>( zcBase );
        float32v wcfBase = FS::Convert<float>( wcBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v xcfOffset = xcf - x;
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v ycfOffset = ycf - y;
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v zcfOffset = zcf - z;
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        xd = FS::FMulAdd( xd, invMag, xcfOffset );
                        yd = FS::FMulAdd( yd, invMag, ycfOffset );
                        zd = FS::FMulAdd( zd, invMag, zcfOffset );
                        wd = FS::FMulAdd( wd, invMag, wcf - w );

                        float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
                        if( sizeJitterActive )
                        {
                            float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                            newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                        }

                        for( int i = COUNT - 1; i > 0; i-- )
                        {
                            distance[i] = FS::Max( FS::Min( distance[i], newDistance ), distance[i - 1] );
                        }

                        distance[0] = FS::Min( distance[0], newDistance );

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return GetReturn( distance, 1 + this->kJitter4D );
    }

    FS_FORCEINLINE float32v GetReturn( std::array<float32v, kMaxDistanceCount>& distance, float maxDist ) const
    {
        if( mDistanceFunction == FastNoise::DistanceFunction::Euclidean )
        {
            distance[mDistanceIndex0] = FastLengthSqrt( distance[mDistanceIndex0] );

            // Avoid double sqrt when both indices are the same
            if( mDistanceIndex1 != mDistanceIndex0 )
            {
                distance[mDistanceIndex1] = FastLengthSqrt( distance[mDistanceIndex1] );
            }
        }

        maxDist *= maxDist;

        switch( mReturnType )
        {
        default:
        case ReturnType::Index0:
        {
            return this->ScaleOutput( distance[mDistanceIndex0], 0, maxDist );
        }
        case ReturnType::Index0Add1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] + distance[mDistanceIndex1], 0, maxDist * 2 );
        }
        case ReturnType::Index0Sub1:
        {
            return this->ScaleOutput( FS::Abs( distance[mDistanceIndex0] - distance[mDistanceIndex1] ), 0, maxDist );
        }
        case ReturnType::Index0Mul1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] * distance[mDistanceIndex1], 0, maxDist * maxDist );
        }
        case ReturnType::Index0Div1:
        {
            return this->ScaleOutput( distance[mDistanceIndex0] * FS::Reciprocal( distance[mDistanceIndex1] ), 0, maxDist );
        }
        }
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<CellularLookup, SIMD> final : public virtual CellularLookup, public DispatchClass<Cellular<Seeded<ScalableGenerator>>, SIMD>
{
    template<typename... P>
    using GenTPtr = float32v( FS_VECTORCALL DispatchClass::* )( int32v, P... ) const;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        auto genT = this->template SelectGenT<1, false>( 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y ) * float32v( -1.f / 0x3ff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY;

        this->ScalePositions( x, y );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = HashPrimesHB( seed, xc, yc );
                float32v xd = FS::Convert<float>( hash & int32v( 0x7ff ) ) - float32v( 0x7ff / 2.0f );
                float32v yd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 21 ) ) - float32v( 0x7ff / 2.0f );

                float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, yd * yd ) );
                float32v localCellX = FS::FMulAdd( xd, invMag, xcf );
                float32v localCellY = FS::FMulAdd( yd, invMag, ycf );
                xd = localCellX - x;
                yd = localCellY - y;

                float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd );
                if( sizeJitterActive )
                {
                    float32v distanceJitter = FS::Convert<float>( ( hash >> 11 ) & int32v( 0x3ff ) );
                    newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                }

                mask32v closer = newDistance < distance;
                distance = FS::Min( newDistance, distance );

                cellX = FS::Select( closer, localCellX, cellX );
                cellY = FS::Select( closer, localCellY, cellY );

                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        auto genT = this->template SelectGenT<1, false>( 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z ) * float32v( -1.f / 0xffff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ;

        this->ScalePositions( x, y, z );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );
        float32v zcfBase = FS::Convert<float>( zcBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS::Convert<float>( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS::Convert<float>( (hash >> 11) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 22 ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, zd * zd ) ) );
                    float32v localCellX = FS::FMulAdd( xd, invMag, xcf );
                    float32v localCellY = FS::FMulAdd( yd, invMag, ycf );
                    float32v localCellZ = FS::FMulAdd( zd, invMag, zcf );
                    xd = localCellX - x;
                    yd = localCellY - y;
                    zd = localCellZ - z;

                    float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd );
                    if( sizeJitterActive )
                    {
                        float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xffff ) );
                        newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                    }

                    mask32v closer = newDistance < distance;
                    distance = FS::Min( newDistance, distance );

                    cellX = FS::Select( closer, localCellX, cellX );
                    cellY = FS::Select( closer, localCellY, cellY );
                    cellZ = FS::Select( closer, localCellZ, cellZ );

                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ), cellZ * float32v( mScale ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        auto genT = this->template SelectGenT<1, false>( 1, []( auto specialised, auto distFunc, auto count ) -> GenTPtr<float32v, float32v, float32v, float32v>
        {
            return &DispatchClass::GenT<decltype( specialised )::value, decltype( distFunc )::value, decltype( count )::value>;
        } );

        return ( this->*genT )( seed, x, y, z, w );
    }

    template<bool SPECIALISED, DistanceFunction DIST_FUNC, int COUNT>
    float32v FS_VECTORCALL GenT( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        // Compile time constants unless this is the generic variant
        DistanceFunction distFunc = SPECIALISED ? DIST_FUNC : mDistanceFunction;

        int32v sourceSeed = seed;
        seed += int32v( this->mSeedOffset );
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mGridJitter, sourceSeed, x, y, z, w );
        float32v minkowskiP = GetMinkowskiP( distFunc, mMinkowskiP, sourceSeed, x, y, z, w );
        float32v sizeJitter;
        bool sizeJitterActive = !SPECIALISED && ( mSizeJitter.simdGeneratorPtr || mSizeJitter.constant != 0.0f );
        if( sizeJitterActive )
        {
            sizeJitter = this->GetSourceValue( mSizeJitter, sourceSeed, x, y, z, w ) * float32v( -1.f / 0xfffff );
        }

        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ, cellW;

        this->ScalePositions( x, y, z, w );

        int32v xc = FS::Convert<int32_t>( x ) + int32v( -1 );
        int32v ycBase = FS::Convert<int32_t>( y ) + int32v( -1 );
        int32v zcBase = FS::Convert<int32_t>( z ) + int32v( -1 );
        int32v wcBase = FS::Convert<int32_t>( w ) + int32v( -1 );

        float32v xcf = FS::Convert<float>( xc );
        float32v ycfBase = FS::Convert<float>( ycBase );
        float32v zcfBase = FS::Convert<float>( zcBase );
        float32v wcfBase = FS::Convert<float>( wcBase );

        xc *= int32v( Primes::X );
        ycBase *= int32v( Primes::Y );
        zcBase *= int32v( Primes::Z );
        wcBase *= int32v( Primes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS::Convert<float>( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS::Convert<float>( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS::Convert<float>( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS::Convert<float>( FS::BitShiftRightZeroExtend( hash, 24 ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS::InvSqrt( FS::FMulAdd( xd, xd, FS::FMulAdd( yd, yd, FS::FMulAdd( zd, zd, wd * wd ) ) ) );
                        float32v localCellX = FS::FMulAdd( xd, invMag, xcf );
                        float32v localCellY = FS::FMulAdd( yd, invMag, ycf );
                        float32v localCellZ = FS::FMulAdd( zd, invMag, zcf );
                        float32v localCellW = FS::FMulAdd( wd, invMag, wcf );
                        xd = localCellX - x;
                        yd = localCellY - y;
                        zd = localCellZ - z;
                        wd = localCellW - w;

                        float32v newDistance = CalcDistance<false>( distFunc, minkowskiP, xd, yd, zd, wd );
                        if( sizeJitterActive )
                        {
                            float32v distanceJitter = FS::Convert<float>( hash & int32v( 0xfffff ) );
                            newDistance *= FS::FNMulAdd( sizeJitter, distanceJitter, float32v( 1 ) );
                        }

                        mask32v closer = newDistance < distance;
                        distance = FS::Min( newDistance, distance );

                        cellX = FS::Select( closer, localCellX, cellX );
                        cellY = FS::Select( closer, localCellY, cellY );
                        cellZ = FS::Select( closer, localCellZ, cellZ );
                        cellW = FS::Select( closer, localCellW, cellW );

                        wcf += float32v( 1 );
                        wc += int32v( Primes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( Primes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( Primes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( Primes::X );
        }

        return this->GetSourceValue( mLookup, sourceSeed - int32v( -1 ), cellX * float32v( mScale ), cellY * float32v( mScale ), cellZ * float32v( mScale ), cellW * float32v( mScale ) );
    }
};
