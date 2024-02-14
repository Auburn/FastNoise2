#pragma once
#include <climits>

namespace FastNoise
{
    namespace Primes
    {
        static constexpr int X = 501125321;
        static constexpr int Y = 1136930381;
        static constexpr int Z = 1720413743;
        static constexpr int W = 1066037191;

        static constexpr int Lookup[] = { X,Y,Z,W };
    }

    static constexpr float kValueBounds = 2147483648.f;
    static constexpr float kRoot2 = 1.4142135623730950488f;
    static constexpr float kRoot3 = 1.7320508075688772935f;

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotSimplex( int32v hash, float32v fX, float32v fY )
    {
        // [0,12) in approximately uniform distribution, with bits 0,1 swapped with 2,3.
        int32v index = ( hash & int32v( ( 1 << 28 ) - 1 ) ) * int32v( ( -4 << 28 ) / 3 ) >> 28;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( kRoot3, kRoot3, 2, 2, 1, -1, 0, 0, -kRoot3, -kRoot3, -2, -2, -1, 1, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, -1, 0, 0, kRoot3, kRoot3, 2, 2, -1, 1, 0, 0, -kRoot3, -kRoot3, -2, -2 ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kRoot3, kRoot3, 2, 2, 1, -1, 0, 0 ), index );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, -1, 0, 0, kRoot3, kRoot3, 2, 2 ), index );

            // Bit-8 = Flip sign of a + b 
            return FS::FMulAdd( gX, fX, fY * gY ) ^ FS::Cast<float>( ( index >> 3 ) << 31 );
        }
        else
        {
            // Bit-3 = Choose X Y ordering
            mask32v bit3 = constexpr( SIMD & FastSIMD::FeatureFlag::SSE2 ) ?
                constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 ) ?
                    FS::Cast<FS::Mask<32>>( index << 29 ) :
                    FS::Cast<FS::Mask<32>>( ( index << 29 ) >> 31 ) :
                ( index & int32v( 1 << 2 ) ) != int32v( 0 );

            float32v a = FS::Select( bit3, fY, fX );
            float32v b = FS::Select( bit3, fX, fY );

            // Bit-1 = b flip sign
            b ^= FS::Cast<float>( index << 31 );

            // Bit-2 = Mul a by 2 or Root3
            mask32v bit2 = ( index & int32v( 2 ) ) == int32v( 0 );

            a *= FS::Select( bit2, float32v( kRoot3 ), float32v( 2 ) );

            // b zero value if a mul 2
            float32v c = FS::MaskedAdd( bit2, a, b );

            // Bit-4 = Flip sign of a + b
            return c ^ FS::Cast<float>( ( index >> 3 ) << 31 );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotSimplex( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW )
    {
        const float SQRT5 = 2.236067977499f;
        const float F4 = ( SQRT5 - 1.0f ) / 4.0f;

        // Bits 26-31 contain [0,20) in approximately uniform distribution.
        int32v index = ( ( hash & int32v( ( 1 << 28 ) - 1 ) ) * int32v( 20 >> 2 ) );

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            index >>= 26;

            const auto tableX = FS::Constant<float>( F4 + 1, F4, F4, F4, -1, 1, 0, 0, -1, 0, 1, 0, -1, 0, 0, 1 );
            const auto tableY = FS::Constant<float>( F4, F4 + 1, F4, F4, 1, -1, 0, 0, 0, -1, 0, 1, 0, -1, 1, 0 );
            const auto tableZ = FS::Constant<float>( F4, F4, F4 + 1, F4, 0, 0, -1, 1, 1, 0, -1, 0, 0, 1, -1, 0 );
            const auto tableW = FS::Constant<float>( F4, F4, F4, F4 + 1, 0, 0, 1, -1, 0, 1, 0, -1, 1, 0, 0, -1 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableX, index, -tableX );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableY, index, -tableY );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableZ, index, -tableZ );
            float32v gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableW, index, -tableW );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
        else
        {
            int32v indexA = index & int32v( 0x03 << 26 );
            int32v indexB = ( index >> 2 ) & int32v( 0x07 << 26 );
            indexB ^= indexA; // Simplifies the AVX512_F case.

            mask32v extra = indexB >= int32v( 0x04 << 26 );
            mask32v equal = ( indexA == indexB );
            indexA |= FS::Cast<int32_t>( equal ); // Forces decrement conditions to fail.

            float32v neutral = FS::Masked( equal | extra, FS::MaskedMul( extra, float32v( F4 ), float32v( -1.0f ) ) );

            float32v gX = FS::MaskedIncrement( indexB == int32v( 0 << 26 ), FS::MaskedDecrement( indexA == int32v( 0 << 26 ), neutral ) );
            float32v gY = FS::MaskedIncrement( indexB == int32v( 1 << 26 ), FS::MaskedDecrement( indexA == int32v( 1 << 26 ), neutral ) );
            float32v gZ = FS::MaskedIncrement( indexB == int32v( 2 << 26 ), FS::MaskedDecrement( indexA == int32v( 2 << 26 ), neutral ) );
            float32v gW = FS::MaskedIncrement( indexB == int32v( 3 << 26 ), FS::MaskedDecrement( indexA == int32v( 3 << 26 ), neutral ) );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotCommon( int32v hash, float32v fX, float32v fY, float32v fZ )
    {
        // [0,12) in approximately uniform distribution.
        int32v index = ( ( hash & int32v( ( 1 << 29 ) - 1 ) ) * int32v( 12 >> 2 ) ) >> 27;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, 0 ) );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gZ, fZ, FS::FMulAdd( fY, gY, fX * gX ) );
        }
        else
        {
            float32v sign1 = FS::Cast<float>( index << 31 );
            float32v sign2 = FS::Cast<float>( ( index >> 1 ) << 31 );

            mask32v thirdCombo = constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 ) ?
                FS::Cast<FS::Mask<32>>( index << ( 31 - 3 ) ) :
                index >= int32v( 8 );

            float32v u = FS::Select( thirdCombo, fY, fX );
            float32v v = FS::Select( index >= int32v( 4 ), fZ, fY );

            return ( u ^ sign1 ) + ( v ^ sign2 );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FS_FORCEINLINE static float32v GetGradientDotPerlin( int32v hash, float32v fX, float32v fY )
    {
        // ( 1+R2, 1 ) ( -1-R2, 1 ) ( 1+R2, -1 ) ( -1-R2, -1 )
        // ( 1, 1+R2 ) ( 1, -1-R2 ) ( -1, 1+R2 ) ( -1, -1-R2 )

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1 + kRoot2, -1 - kRoot2, 1 + kRoot2, -1 - kRoot2, 1, -1, 1, -1, 1 + kRoot2, -1 - kRoot2, 1 + kRoot2, -1 - kRoot2, 1, -1, 1, -1 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2, 1 + kRoot2, -1 - kRoot2, -1 - kRoot2, 1, 1, -1, -1, 1 + kRoot2, 1 + kRoot2, -1 - kRoot2, -1 - kRoot2 ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1 + kRoot2, -1 - kRoot2, 1 + kRoot2, -1 - kRoot2, 1, -1, 1, -1 ), hash );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2, 1 + kRoot2, -1 - kRoot2, -1 - kRoot2 ), hash );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else
        {
            int32v bit1 = hash << 31;
            int32v bit2 = ( hash >> 1 ) << 31;
            int32v bit4 = hash << 29;

            if constexpr( !( SIMD & FastSIMD::FeatureFlag::SSE41 ) )
            {
                bit4 >>= 31;
            }

            auto bit4Mask = FS::Cast<FS::Mask<32, false>>( bit4 );

            fX ^= FS::Cast<float>( bit1 );
            fY ^= FS::Cast<float>( bit2 );

            float32v a = FS::Select( bit4Mask, fY, fX );
            float32v b = FS::Select( bit4Mask, fX, fY );

            return FS::FMulAdd( float32v( 1.0f + kRoot2 ), a, b );
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
            float32v b;
            if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
            {
                b = FS::Select( FS::Cast<FS::Mask<32>>( hash << 27 ), fY, fZ );
            }
            else
            {
                b = FS::Select( p > int32v( 1 << 3 ), fY, fZ );
            }
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
        hash ^= (primedPos ^ ...);

        hash *= int32v( 0x27d4eb2d );
        return (hash >> 15) ^ hash;
    }

    template<typename... P>
    FS_FORCEINLINE static int32v HashPrimesHB( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);
        
        hash *= int32v( 0x27d4eb2d );
        return hash;
    }  

    template<typename... P>
     FS_FORCEINLINE static float32v GetValueCoord( int32v seed, P... primedPos )
    {
        int32v hash = seed;
        hash ^= (primedPos ^ ...);

        hash *= hash * int32v( 0x27d4eb2d );
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

                float32v invSqrt = FS::InvSqrt( distSqr );

                return FS::Masked( invSqrt != float32v( INFINITY ), distSqr * invSqrt );
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
