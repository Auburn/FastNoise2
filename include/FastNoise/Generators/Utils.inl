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

    // The hash, gradient and interpolation helpers are templated on the register type so they also work on the native
    // halves of float32v (.v0/.v1), for generators that evaluate each half separately to reduce register pressure

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotSimplex( I hash31, F fX, F fY )
    {
        I index = FS::BitShiftRightZeroExtend( hash31, 1 ) * I( 12 >> 2 ); // [0,12) in the upper four bits

        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            index >>= 28;

            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( kRoot3f, -kRoot3f, 1, -1, kRoot3f, -kRoot3f, -1, 1, 2, -2, 0, 0, 0, 0, 0, 0 ) );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( 1, -1, kRoot3f, -kRoot3f, -1, 1, kRoot3f, -kRoot3f, 0, 0, 2, -2, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 )
        {
            F finalSign = FS::Cast<float>( ( index >> 28 ) << 31 );
            index >>= 29;

            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), index );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), index );

            return FS::FMulAdd( gX, fX, fY * gY ) ^ finalSign;
        }
        else
        {
            F u = FS::SelectHighBit( index << 2, fY, fX );
            F v = FS::SelectHighBit( index << 2, fX, fY );

            F a = u * FS::SelectHighBit( index, F( 2 ), F( kRoot3f ) );
            F b = v ^ FS::Cast<float>( ( index >> 30 ) << 31 );

            if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::x86 )
            {
                auto indexNegativeMask = FS::Cast<FS::Mask<32, false>>( index >> 31 );

                return FS::InvMaskedAdd( indexNegativeMask, a, b ) ^ FS::Cast<float>( ( index >> 28 ) << 31 );
            }
            else
            {
                return FS::MaskedAdd( index >= I( 0 ), a, b ) ^ FS::Cast<float>( ( index >> 28 ) << 31 );
            }
        }
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotSimplex( I hash31, F fX, F fY, F fZ, F fW )
    {
        I hashShifted = FS::BitShiftRightZeroExtend( hash31, 2 );
        I index = hashShifted * I( 20 >> 2 ); // [0,20) in the upper five bits

        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            index = FS::BitShiftRightZeroExtend( index, 27 );

            const auto tableX = FS::Constant<float>( kSkew4f + 1, kSkew4f, kSkew4f, kSkew4f, -1, 1, 0, 0, -1, 0, 1, 0, -1, 0, 0, 1 );
            const auto tableY = FS::Constant<float>( kSkew4f, kSkew4f + 1, kSkew4f, kSkew4f, 1, -1, 0, 0, 0, -1, 0, 1, 0, -1, 1, 0 );
            const auto tableZ = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f + 1, kSkew4f, 0, 0, -1, 1, 1, 0, -1, 0, 0, 1, -1, 0 );
            const auto tableW = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f, kSkew4f + 1, 0, 0, 1, -1, 0, 1, 0, -1, 1, 0, 0, -1 );

            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableX, index, -tableX );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableY, index, -tableY );
            F gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableZ, index, -tableZ );
            F gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableW, index, -tableW );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
        else
        {
            I indexA = index & I( 0x03 << 27 );
            I indexB = ( index >> 2 ) & I( 0x07 << 27 );
            indexB ^= indexA; // Simplifies the AVX512_F case.

            typename F::MaskType extra = indexB >= I( 0x04 << 27 );
            typename F::MaskType equal = ( indexA == indexB );
            indexA |= FS::Cast<int32_t>( equal ); // Forces decrement conditions to fail.

            F neutral = FS::Masked( equal | extra, FS::MaskedMul( extra, F( kSkew4f ), F( -1.0f ) ) );

            F gX = FS::MaskedIncrement( indexB == I( 0 << 27 ), FS::MaskedDecrement( indexA == I( 0 << 27 ), neutral ) );
            F gY = FS::MaskedIncrement( indexB == I( 1 << 27 ), FS::MaskedDecrement( indexA == I( 1 << 27 ), neutral ) );
            F gZ = FS::MaskedIncrement( indexB == I( 2 << 27 ), FS::MaskedDecrement( indexA == I( 2 << 27 ), neutral ) );
            F gW = FS::MaskedIncrement( indexB == I( 3 << 27 ), FS::MaskedDecrement( indexA == I( 3 << 27 ), neutral ) );

            return FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
        }
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotCommon( I hash, F fX, F fY, F fZ )
    {
        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 ) );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            F gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, fZ * gZ ));
        }
        else
        {
            I hasha13 = hash & I( 13 );

            // if h > 7 then y, else x
            typename F::MaskType gt7;
            if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::SSE41 )
            {
                gt7 = FS::Cast<FS::Mask<32>>( hash << 28 );
            }
            else
            {
                gt7 = hasha13 > I( 7 );
            }
            F u = FS::Select( gt7, fY, fX );

            // if h < 4 then y else if h is 12 or 14 then x else z
            F v = FS::Select( hasha13 == I( 12 ), fX, fZ );
            v = FS::Select( hasha13 < I( 2 ), fY, v );

            if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 )
            {
                // Look up the sign bits as +/-1 and apply them with mul/FMA
                F h1 = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1 ), hash );
                F h2 = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1 ), hash );

                return FS::FMulAdd( v, h2, u * h1 );
            }
            else
            {
                // if h1 then -u else u
                // if h2 then -v else v
                F h1 = FS::Cast<float>( hash << 31 );
                F h2 = FS::Cast<float>( ( hash >> 1 ) << 31 );
                // then add them
                return ( u ^ h1 ) + ( v ^ h2 );
            }
        }
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotPerlin( I hash, F fX, F fY )
    {
        // ( 1+R2, 1 ) ( -1-R2, 1 ) ( 1+R2, -1 ) ( -1-R2, -1 )
        // ( 1, 1+R2 ) ( 1, -1-R2 ) ( -1, 1+R2 ) ( -1, -1-R2 )

        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1, 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1 ) );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hash, FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f, 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f ) );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 )
        {
            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1 + kRoot2f, -1 - kRoot2f, 1 + kRoot2f, -1 - kRoot2f, 1, -1, 1, -1 ), hash );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, 1, -1, -1, 1 + kRoot2f, 1 + kRoot2f, -1 - kRoot2f, -1 - kRoot2f ), hash );

            return FS::FMulAdd( gX, fX, fY * gY );
        }
        else
        {
            fX ^= FS::Cast<float>( hash << 31 );
            fY ^= FS::Cast<float>( ( hash >> 1 ) << 31 );

            F u = FS::SelectHighBit( hash << 29, fY, fX );
            F v = FS::SelectHighBit( hash << 29, fX, fY );

            return FS::FMulAdd( F( 1.0f + kRoot2f ), u, v );
        }
    }

    // AVX512 Perlin 4D gradient components, see GetGradientDotPerlin
    template<typename F, typename I>
    FS_FORCEINLINE static void GetGradientPerlin4DAVX512( I hash, F& gX, F& gY, F& gZ, F& gW )
    {
        gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ), hash, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 ) );
        gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ), hash, FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ) );

        // gZ and gW don't depend on hash bit 0, so they only need single table permutes
        I hashShifted = hash >> 1;
        gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hashShifted, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 1, -1, -1 ) );
        gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), hashShifted, FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) );
    }

    // AVX2 Perlin 4D gradient signs (+/-1) of the a/b/c components, see GetGradientDotPerlin
    template<typename F, typename I>
    FS_FORCEINLINE static void GetGradientSignsPerlin4DAVX2( I hash, F& aSign, F& bSign, F& cSign )
    {
        aSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1 ), hash );
        bSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1 ), hash );
        cSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1 ), hash );
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotPerlin( I hash, F fX, F fY, F fZ, F fW )
    {
        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            F gX, gY, gZ, gW;
            GetGradientPerlin4DAVX512( hash, gX, gY, gZ, gW );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, FS::FMulAdd( fZ, gZ, fW * gW ) ));
        }
        else
        {
            I p = hash & I( 3 << 3 );

            F a = FS::Select( p > I( 0 ), fX, fY );
            F b = FS::SelectHighBit( hash << 27, fY, fZ );
            F c = FS::Select( p > I( 2 << 3 ), fZ, fW );

            if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 )
            {
                // Look up the sign bits as +/-1 and apply them with mul/FMA
                F aSign, bSign, cSign;
                GetGradientSignsPerlin4DAVX2( hash, aSign, bSign, cSign );

                return FS::FMulAdd( c, cSign, FS::FMulAdd( b, bSign, a * aSign ) );
            }
            else
            {
                F aSign = FS::Cast<float>( hash << 31 );
                F bSign = FS::Cast<float>( ( hash >> 1 ) << 31 );
                F cSign = FS::Cast<float>( ( hash >> 2 ) << 31 );

                return ( a ^ aSign ) + ( b ^ bSign ) + ( c ^ cSign );
            }
        }
    }

    // x + y + z of scaled positions for the 3D skew. MSVC 19.5x (VS 2026) /fp:fast miscompiles k * ( x * f + y * f + z * f ) on
    // 2x wide registers: it factors f out of the sum but leaves it on z. Summing y + z first avoids it
    template<typename F>
    FS_FORCEINLINE static F SkewSum( F x, F y, F z )
    {
#if defined( _MSC_VER ) && !defined( __clang__ ) && _MSC_VER >= 1950
        return x + ( y + z );
#else
        return x + y + z;
#endif
    }

    // Equivalent to FS::Convert<int32_t>( floored ), on AVX512 the conversion rounds down directly so it doesn't have to wait on the floor
    template<typename F>
    FS_FORCEINLINE static auto FloorToInt( F value, F floored )
    {
        using I = decltype( FS::Convert<std::int32_t>( floored ) );

        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::NativeExec<I>( []( auto a ) { return _mm512_cvt_roundps_epi32( a, 0x09 ); }, value ); // _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC
        }
        else
        {
            return FS::Convert<std::int32_t>( floored );
        }
    }

    template<typename I, typename... P>
    FS_FORCEINLINE static I HashPrimes( I seed, P... primedPos )
    {
        I hash = seed;
        hash ^= ( primedPos ^ ... );

        hash *= I( HashMultiplier::A );

        return ( hash >> 15 ) ^ hash;
    }

    template<typename I, typename... P>
    FS_FORCEINLINE static I HashPrimesHB( I seed, P... primedPos )
    {
        I hash = seed;
        hash ^= ( primedPos ^ ... );

        hash *= I( HashMultiplier::A );
        return hash;
    }

    template<typename I, typename... P>
    FS_FORCEINLINE static auto GetValueCoord( I seed, P... primedPos )
    {
        I hash = seed;
        hash ^= (primedPos ^ ...);

        if constexpr( ( I::FeatureFlags & FastSIMD::FeatureFlag::x86 ) && !( I::FeatureFlags & FastSIMD::FeatureFlag::SSE41 ) )
        {
            // SSE2 has no 32 bit multiply, each one is emulated with pmuludq on the even and odd lanes plus a repack.
            // pmuludq only reads the low half of each 64 bit lane, so both multiplies can run before a single repack
            hash = FS::NativeExec<I>( []( auto h, auto a )
            {
                auto hOdd = _mm_srli_epi64( h, 32 );
                auto even = _mm_mul_epu32( _mm_mul_epu32( h, h ), a );
                auto odd = _mm_mul_epu32( _mm_mul_epu32( hOdd, hOdd ), a );

                // [ e0, e2, o1, o3 ] -> [ e0, o1, e2, o3 ]
                return _mm_shuffle_epi32( _mm_castps_si128( _mm_shuffle_ps( _mm_castsi128_ps( even ), _mm_castsi128_ps( odd ), 0x88 ) ), 0xD8 );
            }, hash, I( HashMultiplier::A ) );
        }
        else
        {
            hash *= hash * I( HashMultiplier::A );
        }
        return FS::Convert<float>( hash );
    }

    template<typename F>
    FS_FORCEINLINE static F Lerp( F a, F b, F t )
    {
        return FS::FMulAdd( t, b - a, a );
    }

    template<typename F>
    FS_FORCEINLINE static F InterpHermite( F t )
    {
        return t * t * FS::FNMulAdd( t, F( 2 ), F( 3 ) );
    }

    template<typename F>
    FS_FORCEINLINE static F InterpQuintic( F t )
    {
        return t * t * t * FS::FMulAdd( t, FS::FMulAdd( t, F( 6 ), F( -15 ) ), F( 10 ) );
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

    // Sample Minkowski P once at the generator's input position, not per distance calculation
    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault(), typename... P>
    FS_FORCEINLINE static float32v GetMinkowskiP( DistanceFunction distFunc, const HybridSource& minkowskiP, int32v seed, P... pos )
    {
        // Avoid evaluating a connected source when it isn't used
        if( distFunc != DistanceFunction::Minkowski )
        {
            return float32v( 0 );
        }

        return FastSIMD::DispatchClass<Generator, SIMD>::GetSourceValue( minkowskiP, seed, pos... );
    }

    template<bool DO_SQRT = true, typename... P>
    FS_FORCEINLINE static float32v CalcDistance( DistanceFunction distFunc, float32v minkowskiP, float32v pX, P... pos )
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
                float32v sum = FS::Pow( FS::Abs( pX ), minkowskiP );
                ((sum += FS::Pow( FS::Abs( pos ), minkowskiP )), ...);

                return FS::Pow( sum, FS::Reciprocal( minkowskiP ) );
            }
        }
    }    
}
