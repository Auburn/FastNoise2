#pragma once
#include <climits>

namespace FastNoise
{
    namespace Primes
    {
        static constexpr int X = (int)0xF797C5C7;
        static constexpr int Y = (int)0x6C060C89;
        static constexpr int Z = (int)0x465FD04F;
        static constexpr int W = (int)0xF7A62279;

        static constexpr int Lookup[] = { X,Y,Z,W };
    }

    namespace HashMultiplier
    {
        static constexpr int A = (int)0xB7E0A5F5;
    };

    static constexpr double kRoot2 = 1.4142135623730950488016887242097;
    static constexpr double kRoot3 = 1.7320508075688772935274463415059;
    static constexpr double kRoot5 = 2.2360679774997896964091736687313;
    static constexpr double kSkew2 = 1.0 / ( kRoot3 + 1.0 );
    static constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );

    static constexpr float kValueBounds = 2147483648.f;
    static constexpr float kRoot2f = kRoot2;
    static constexpr float kRoot3f = kRoot3;
    static constexpr float kSkew2f = kSkew2;
    static constexpr float kSkew4f = kSkew4;

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotSimplex( int32v hash31, float32v fX, float32v fY )
    {
        int32v index = FS::BitShiftRightZeroExtend( hash31, 1 ) * int32v( 12 >> 2 ); // [0,12) in the upper four bits

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            index >>= 28;

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( kRoot3f, -kRoot3f, 1, -1, kRoot3f, -kRoot3f, -1, 1, 2, -2, 0, 0, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, -1, kRoot3f, -kRoot3f, -1, 1, kRoot3f, -kRoot3f, 0, 0, 2, -2, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v finalSign = FS::Cast<float>( ( index >> 28 ) << 31 );
            index >>= 29;

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), index );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), index );

            return FS::FMulAdd( gX, fX, fY * gY ) ^ finalSign;
        }
        else
        {
            float32v u = FS::SelectHighBit( index << 2, fY, fX );
            float32v v = FS::SelectHighBit( index << 2, fX, fY );

            float32v a = u * FS::SelectHighBit( index, float32v( 2 ), float32v( kRoot3f ) );
            float32v b = v ^ FS::Cast<float>( ( index >> 30 ) << 31 );

            if constexpr( SIMD & FastSIMD::FeatureFlag::x86 )
            {
                auto indexNegativeMask = FS::Cast<FS::Mask<32, false>>( index >> 31 );

                return FS::InvMaskedAdd( indexNegativeMask, a, b ) ^ FS::Cast<float>( ( index >> 28 ) << 31 );
            }
            else
            {
                return FS::MaskedAdd( index >= int32v( 0 ), a, b ) ^ FS::Cast<float>( ( index >> 28 ) << 31 );
            }
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotSimplex( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v fW )
    {
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 2 );
        int32v index = hashShifted * int32v( 20 >> 2 ); // [0,20) in the upper five bits

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            index = FS::BitShiftRightZeroExtend( index, 27 );

            const auto tableX = FS::Constant<float>( kSkew4f + 1, kSkew4f, kSkew4f, kSkew4f, -1, 1, 0, 0, -1, 0, 1, 0, -1, 0, 0, 1 );
            const auto tableY = FS::Constant<float>( kSkew4f, kSkew4f + 1, kSkew4f, kSkew4f, 1, -1, 0, 0, 0, -1, 0, 1, 0, -1, 1, 0 );
            const auto tableZ = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f + 1, kSkew4f, 0, 0, -1, 1, 1, 0, -1, 0, 0, 1, -1, 0 );
            const auto tableW = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f, kSkew4f + 1, 0, 0, 1, -1, 0, 1, 0, -1, 1, 0, 0, -1 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableX, index, -tableX );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableY, index, -tableY );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableZ, index, -tableZ );
            float32v gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableW, index, -tableW );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
        else
        {
            int32v indexA = index & int32v( 0x03 << 27 );
            int32v indexB = ( index >> 2 ) & int32v( 0x07 << 27 );
            indexB ^= indexA; // Simplifies the AVX512_F case.

            mask32v extra = indexB >= int32v( 0x04 << 27 );
            mask32v equal = ( indexA == indexB );
            indexA |= FS::Cast<int32_t>( equal ); // Forces decrement conditions to fail.

            float32v neutral = FS::Masked( equal | extra, FS::MaskedMul( extra, float32v( kSkew4f ), float32v( -1.0f ) ) );

            float32v gX = FS::MaskedIncrement( indexB == int32v( 0 << 27 ), FS::MaskedDecrement( indexA == int32v( 0 << 27 ), neutral ) );
            float32v gY = FS::MaskedIncrement( indexB == int32v( 1 << 27 ), FS::MaskedDecrement( indexA == int32v( 1 << 27 ), neutral ) );
            float32v gZ = FS::MaskedIncrement( indexB == int32v( 2 << 27 ), FS::MaskedDecrement( indexA == int32v( 2 << 27 ), neutral ) );
            float32v gW = FS::MaskedIncrement( indexB == int32v( 3 << 27 ), FS::MaskedDecrement( indexA == int32v( 3 << 27 ), neutral ) );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotCommon( int32v hash, float32v fX, float32v fY, float32v fZ )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, fZ * gZ ));
        }
        else
        {
            int32v hasha13 = hash & int32v( 13 );

            // if h > 7 then y, else x
            mask32v gt7;
            if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
            {
                gt7 = FS::Cast<FS::Mask<32>>( hash << 28 );
            }
            else
            {
                gt7 = hasha13 > int32v( 7 );
            }
            float32v u = FS::Select( gt7, fY, fX );

            // if h < 4 then y else if h is 12 or 14 then x else z
            float32v v = FS::Select( hasha13 == int32v( 12 ), fX, fZ );
            v = FS::Select( hasha13 < int32v( 2 ), fY, v );

            // if h1 then -u else u
            // if h2 then -v else v
            float32v h1 = FS::Cast<float>( hash << 31 );
            float32v h2 = FS::Cast<float>( ( hash >> 1 ) << 31 );
            // then add them
            return ( u ^ h1 ) + ( v ^ h2 );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotPerlin( int32v hash, float32v fX, float32v fY )
    {
        // ( 1+R2, 1 ) ( -1-R2, 1 ) ( 1+R2, -1 ) ( -1-R2, -1 )
        // ( 1, 1+R2 ) ( 1, -1-R2 ) ( -1, 1+R2 ) ( -1, -1-R2 )

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1, 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f, 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1 ), hash );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f ), hash );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else
        {
            fX ^= FS::Cast<float>( hash << 31 );
            fY ^= FS::Cast<float>( ( hash >> 1 ) << 31 );

            float32v u = FS::SelectHighBit( hash << 29, fY, fX );
            float32v v = FS::SelectHighBit( hash << 29, fX, fY );

            return FS::FMulAdd( float32v( 1.0f + kRoot2f ), u, v );
        }
    }
    
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotPerlin( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ), hash, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ), hash, FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ) );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ), hash, FS::Constant<float>( 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, -1, -1, -1, -1 ) );
            float32v gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1, 1, -1, -1, -1, -1 ), hash, FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, FS::FMulAdd( fZ, gZ, fW * gW ) ));
        }
        else
        {
            int32v p = hash & int32v( 3 << 3 );

            float32v a = FS::Select( p > int32v( 0 ), fX, fY );
            float32v b = FS::SelectHighBit( hash << 27, fY, fZ );
            float32v c = FS::Select( p > int32v( 2 << 3 ), fZ, fW );

            float32v aSign = FS::Cast<float>( hash << 31 );
            float32v bSign = FS::Cast<float>( ( hash >> 1 ) << 31 );
            float32v cSign = FS::Cast<float>( ( hash >> 2 ) << 31 );

            return ( a ^ aSign ) + ( b ^ bSign ) + ( c ^ cSign );
        }
    }

    template<typename... P>
    FS_FORCEINLINE static int32v HashPrimes( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= ( primedPos ^ ... );

        hash *= int32v( HashMultiplier::A );

        return ( hash >> 15 ) ^ hash;
    }

    template<typename... P>
    FS_FORCEINLINE static int32v HashPrimesHB( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= ( primedPos ^ ... );
        
        hash *= int32v( HashMultiplier::A );
        return hash;
    }

    template<typename... P>
    FS_FORCEINLINE static float32v GetValueCoord( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);

        hash *= hash * int32v( HashMultiplier::A );
        return FS::Convert<float>( hash );
    }
     
    FS_FORCEINLINE static float32v Lerp( float32v a, float32v b, float32v t )
    {
        return FS::FMulAdd( t, b - a, a );
    }
    
    FS_FORCEINLINE static float32v InterpHermite( float32v t )
    {
        return t * t * FS::FNMulAdd( t, float32v( 2 ), float32v( 3 ));
    }
     
    FS_FORCEINLINE static float32v InterpQuintic( float32v t )
    {
        return t * t * t * FS::FMulAdd( t, FS::FMulAdd( t, float32v( 6 ), float32v( -15 )), float32v( 10 ) );
    }

    FS_FORCEINLINE static float32v FastLengthSqrt( float32v sqrDist )
    {
        if constexpr( FastSIMD::IsRelaxed() )
        {
            float32v invSqrt = FS::InvSqrt( sqrDist );

            return FS::Masked( sqrDist != float32v( 0 ), sqrDist * invSqrt );
        }
        else
        {
            return FS::Sqrt( sqrDist );
        }
    }

    template<bool DO_SQRT = true, FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault(), typename... P>
    FS_FORCEINLINE static float32v CalcDistance( DistanceFunction distFunc, const HybridSource& minkowskiP, int32v seed, float32v pX, P... pos )
    {
        switch( distFunc )
        {
            default:
            case DistanceFunction::Euclidean:
            if constexpr( DO_SQRT )
            {
                float32v distSqr = pX * pX;
                ((distSqr = FS::FMulAdd( pos, pos, distSqr )), ...);

                return FastLengthSqrt( distSqr );
            }

            case DistanceFunction::EuclideanSquared:
            {
                float32v distSqr = pX * pX;
                ((distSqr = FS::FMulAdd( pos, pos, distSqr )), ...);

                return distSqr;
            }

            case DistanceFunction::Manhattan:
            {
                float32v dist = FS::Abs( pX );
                dist += (FS::Abs( pos ) + ...);

                return dist;
            }

            case DistanceFunction::Hybrid:
            {
                float32v both = FS::FMulAdd( pX, pX, FS::Abs( pX ) );
                ((both += FS::FMulAdd( pos, pos, FS::Abs( pos ) )), ...);

                return both;
            }

            case DistanceFunction::MaxAxis:
            {
                float32v max = FS::Abs( pX );
                ((max = FS::Max( FS::Abs( pos ), max )), ...);

                return max;
            }

            case DistanceFunction::Minkowski:
            {
                float32v minkowski = FastSIMD::DispatchClass<Generator, SIMD>::GetSourceValue( minkowskiP, seed, pX, pos... );

                return FS::Pow( FS::Pow( FS::Abs( pX ), minkowski) + (FS::Pow( FS::Abs( pos ), minkowski) + ...), FS::Reciprocal( minkowski ) );
            }
        }
    }    
}
