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

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyGradientOuterProductVectorProductSimplex( int32v hash31, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY )
    {
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 1 );
        int32v indexGradient = hashShifted * int32v( 12 >> 2 ); // [0,12) in the upper four bits
        int32v indexOuterVector = ( hashShifted * int32v( 0xAAAAAAAB ) ) & int32v( 0xC0000003 ); // [0,12) in bits 0,1,30,31 // ( -4LL << 30 ) / 3 )

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexGradient >>= 28;
            indexOuterVector |= indexOuterVector >> 28;

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( kRoot3f, -kRoot3f, 1, -1, kRoot3f, -kRoot3f, -1, 1, 2, -2, 0, 0, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 1, -1, kRoot3f, -kRoot3f, -1, 1, kRoot3f, -kRoot3f, 0, 0, 2, -2, 0, 0, 0, 0 ) );

            multiplier *= FS::FMulAdd( fY, gY, fX * gX );

            valueX = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( kRoot3f, -kRoot3f, 1, -1, kRoot3f, -kRoot3f, -1, 1, 2, -2, 0, 0, 0, 0, 0, 0 ) ), valueX );
            valueY = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 1, -1, kRoot3f, -kRoot3f, -1, 1, kRoot3f, -kRoot3f, 0, 0, 2, -2, 0, 0, 0, 0 ) ), valueY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX )
        {
            float32v finalSign = FS::Cast<float>( ( ( indexGradient >> 28 ) ^ indexOuterVector ) << 31 );
            indexGradient >>= 29;
            indexOuterVector = ( indexOuterVector >> 1 ) | ( indexOuterVector >> 29 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), indexGradient );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), indexGradient );

            multiplier *= FS::FMulAdd( fY, gY, fX * gX ) ^ finalSign;

            valueX = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), indexOuterVector ), valueX );
            valueY = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), indexOuterVector ), valueY );
        }
        else
        {
            {
                float32v u = FS::SelectHighBit( indexGradient << 2, fY, fX );
                float32v v = FS::SelectHighBit( indexGradient << 2, fX, fY );

                float32v a = u * FS::SelectHighBit( indexGradient, float32v( 2 ), float32v( kRoot3f ) );
                float32v b = v ^ FS::Cast<float>( ( indexGradient >> 30 ) << 31 );

                multiplier *= FS::MaskedAdd( indexGradient >= int32v( 0 ), a, b ) ^ FS::Cast<float>( ( ( indexGradient >> 28 ) ^ indexOuterVector ) << 31 );
            }

            {
                float32v a = multiplier * FS::SelectHighBit( indexOuterVector, float32v( 2 ), float32v( kRoot3f ) );
                float32v b = FS::Masked( indexOuterVector >= int32v( 0 ), multiplier ) ^ FS::Cast<float>( ( indexOuterVector >> 30 ) << 31 );

                valueX += FS::SelectHighBit( indexOuterVector << 30, b, a );
                valueY += FS::SelectHighBit( indexOuterVector << 30, a, b );
            }
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyGradientOuterProductVectorProductCommon( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ )
    {
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 1 );
        int32v indexGradient = FS::BitShiftRightZeroExtend( hashShifted * int32v( 12 >> 2 ), 28 ); // [0,12)
        int32v indexOuterVector = ( hashShifted * int32v( 0xAAAAAAAB ) ) & int32v( 0xC0000003 ); // [0,12) in bits 0,1,30,31 // ( -4LL << 30 ) / 3 )
        indexOuterVector |= indexOuterVector >> 28;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, 0 ) );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexGradient, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) );

            multiplier *= FS::FMulAdd( gZ, fZ, FS::FMulAdd( fY, gY, fX * gX ) );

            valueX = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) ), valueX );
            valueY = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, 0 ) ), valueY );
            valueZ = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexOuterVector, FS::Constant<float>( 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0 ) ), valueZ );
        }
        else
        {
            {
                float32v sign0 = FS::Cast<float>( indexGradient << 31 );
                float32v sign1 = FS::Cast<float>( ( indexGradient >> 1 ) << 31 );

                float32v u;
                if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
                {
                    u = FS::SelectHighBit( indexGradient << ( 31 - 3 ), fY, fX );
                }
                else
                {
                    u = FS::Select( indexGradient >= int32v( 8 ), fY, fX );
                }
                float32v v = FS::Select( indexGradient >= int32v( 4 ), fZ, fY );

                multiplier *= ( u ^ sign0 ) + ( v ^ sign1 );
            }

            {
                indexOuterVector &= int32v( 0xF );

                float32v signed0 = multiplier ^ FS::Cast<float>( indexOuterVector << 31 );
                float32v signed1 = multiplier ^ FS::Cast<float>( ( indexOuterVector >> 1 ) << 31 );

                mask32v notYZ = indexOuterVector < int32v( 8 );
                mask32v notXY = indexOuterVector >= int32v( 4 );

                valueX = FS::MaskedAdd( notYZ, valueX, signed0 );
                valueZ = FS::MaskedAdd( notXY, valueZ, signed1 );
                valueY = FS::InvMaskedAdd( notYZ & notXY, valueY, FS::Select( notXY, signed0, signed1 ) );
            }
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyGradientOuterProductVectorProductSimplex( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW )
    {
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash, 2 );
        int32v indexGradient = hashShifted * int32v( 20 >> 2 ); // [0,20) in the upper five bits
        int32v indexOuterVector = hashShifted * int32v( 0xCCCCCCCD ); // ( -8LL << 29 ) / 5
        indexOuterVector = ( indexOuterVector & int32v( 0xE0000003 ) ) * int32v( 3 | ( 1 << 27 ) ); // [0,20) in the upper five bits, independently of the above

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexGradient = FS::BitShiftRightZeroExtend( indexGradient, 27 );
            indexOuterVector = FS::BitShiftRightZeroExtend( indexOuterVector, 27 );

            const auto tableX = FS::Constant<float>( kSkew4f + 1, kSkew4f, kSkew4f, kSkew4f, -1, 1, 0, 0, -1, 0, 1, 0, -1, 0, 0, 1 );
            const auto tableY = FS::Constant<float>( kSkew4f, kSkew4f + 1, kSkew4f, kSkew4f, 1, -1, 0, 0, 0, -1, 0, 1, 0, -1, 1, 0 );
            const auto tableZ = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f + 1, kSkew4f, 0, 0, -1, 1, 1, 0, -1, 0, 0, 1, -1, 0 );
            const auto tableW = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f, kSkew4f + 1, 0, 0, 1, -1, 0, 1, 0, -1, 1, 0, 0, -1 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableX, indexGradient, -tableX );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableY, indexGradient, -tableY );
            float32v gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableZ, indexGradient, -tableZ );
            float32v gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableW, indexGradient, -tableW );

            multiplier *= FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );

            valueX = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableX, indexOuterVector, -tableX ), valueX );
            valueY = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableY, indexOuterVector, -tableY ), valueY );
            valueZ = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableZ, indexOuterVector, -tableZ ), valueZ );
            valueW = FS::FMulAdd( multiplier, FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableW, indexOuterVector, -tableW ), valueW );
        }
        else
        {
            {
                int32v indexA = indexGradient & int32v( 0x03 << 27 );
                int32v indexB = ( indexGradient >> 2 ) & int32v( 0x07 << 27 );
                indexB ^= indexA; // Simplifies the AVX512_F case.

                mask32v extra = indexB >= int32v( 0x04 << 27 );
                mask32v equal = ( indexA == indexB );
                indexA |= FS::Cast<int32_t>( equal ); // Forces decrement conditions to fail.

                float32v neutral = FS::Masked( equal | extra, FS::MaskedMul( extra, float32v( kSkew4f ), float32v( -1.0f ) ) );

                float32v gX = FS::MaskedIncrement( indexB == int32v( 0 << 27 ), FS::MaskedDecrement( indexA == int32v( 0 << 27 ), neutral ) );
                float32v gY = FS::MaskedIncrement( indexB == int32v( 1 << 27 ), FS::MaskedDecrement( indexA == int32v( 1 << 27 ), neutral ) );
                float32v gZ = FS::MaskedIncrement( indexB == int32v( 2 << 27 ), FS::MaskedDecrement( indexA == int32v( 2 << 27 ), neutral ) );
                float32v gW = FS::MaskedIncrement( indexB == int32v( 3 << 27 ), FS::MaskedDecrement( indexA == int32v( 3 << 27 ), neutral ) );

                multiplier *= FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) );
            }

            {
                int32v indexA = indexOuterVector & int32v( 0x03 << 27 );
                int32v indexB = ( indexOuterVector >> 2 ) & int32v( 0x07 << 27 );
                indexB ^= indexA; // Simplifies the AVX512_F case.

                mask32v extra = indexB >= int32v( 0x04 << 27 );
                mask32v equal = ( indexA == indexB );
                indexA |= FS::Cast<int32_t>( equal ); // Forces decrement conditions to fail.

                float32v neutral = FS::Masked( equal | extra, FS::MaskedMul( extra, float32v( kSkew4f ), float32v( -1.0f ) ) );

                float32v gX = FS::MaskedIncrement( indexB == int32v( 0 << 27 ), FS::MaskedDecrement( indexA == int32v( 0 << 27 ), neutral ) );
                float32v gY = FS::MaskedIncrement( indexB == int32v( 1 << 27 ), FS::MaskedDecrement( indexA == int32v( 1 << 27 ), neutral ) );
                float32v gZ = FS::MaskedIncrement( indexB == int32v( 2 << 27 ), FS::MaskedDecrement( indexA == int32v( 2 << 27 ), neutral ) );
                float32v gW = FS::MaskedIncrement( indexB == int32v( 3 << 27 ), FS::MaskedDecrement( indexA == int32v( 3 << 27 ), neutral ) );

                valueX = FS::FMulAdd( multiplier, gX, valueX );
                valueY = FS::FMulAdd( multiplier, gY, valueY );
                valueZ = FS::FMulAdd( multiplier, gZ, valueZ );
                valueW = FS::FMulAdd( multiplier, gW, valueW );
            }
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductSimplex( int32v hash31, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY )
    {
        int32v index = FS::BitShiftRightZeroExtend( hash31, 1 ) * int32v( 12 >> 2 ); // [0,12) in the upper four bits

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            index = FS::BitShiftRightZeroExtend( index, 28 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( kSkew2f, -kSkew2f, kSkew2f, -kSkew2f, kSkew2f + 1, -kSkew2f - 1, kSkew2f + 1, -kSkew2f - 1, 1, -1, 1, -1, 0, 0, 0, 0 ) );
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), index, FS::Constant<float>( kSkew2f + 1, kSkew2f + 1, -kSkew2f - 1, -kSkew2f - 1, kSkew2f, kSkew2f, -kSkew2f, -kSkew2f, 1, 1, -1, -1, 0, 0, 0, 0 ) );

            valueX = FS::FMulAdd( multiplier, FS::FMulAdd( fY, gY, fX * gX ), valueX );
            multiplier ^= FS::Cast<float>( hash31 << 31 );
            valueY = FS::FMulAdd( multiplier, FS::FMulSub( fY, gX, fX * gY ), valueY );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            float32v signX = FS::Cast<float>( ( index >> 28 ) << 31 );
            index = FS::BitShiftRightZeroExtend( index, 29 );

            float32v gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kSkew2f, kSkew2f, kSkew2f + 1, kSkew2f + 1, 1, 1, 0, 0 ), index ) ^ signX;
            float32v gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( kSkew2f + 1, -kSkew2f - 1, kSkew2f, -kSkew2f, 1, -1, 0, 0 ), index );

            valueX = FS::FMulAdd( multiplier, FS::FMulAdd( fY, gY, fX * gX ), valueX );
            multiplier ^= FS::Cast<float>( hash31 << 31 );
            valueY = FS::FMulAdd( multiplier, FS::FMulSub( fY, gX, fX * gY ), valueY );
        }
        else
        {
            int32v ofThree = FS::BitShiftRightZeroExtend( index, 30 );
            float32v signX = FS::Cast<float>( ( index >> 28 ) << 31 );
            float32v signY = FS::Cast<float>( ( index >> 29 ) << 31 );

            float32v masked = FS::Masked( index >= int32v( 0 ), float32v( kSkew2f ) );
            float32v gX = FS::MaskedIncrement( ofThree != int32v( 0 ), masked ) ^ signX;
            float32v gY = FS::MaskedIncrement( ofThree != int32v( 1 ), masked ) ^ signY;

            valueX = FS::FMulAdd( multiplier, FS::FMulAdd( fY, gY, fX * gX ), valueX );
            multiplier ^= FS::Cast<float>( hash31 << 31 );
            valueY = FS::FMulAdd( multiplier, FS::FMulSub( fY, gX, fX * gY ), valueY );
        }
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductCommon( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ )
    {
        constexpr float kComponentA = 2.224744871391589f;
        constexpr float kComponentB = -0.224744871391589f;
        constexpr float kComponentC = -1.0f;
        constexpr float kComponentsDE = 1.0f;
        constexpr float kComponentF = 2.0f;
        
        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 1 );
        int32v indexFacetBasisWithPermute2 = hashShifted * int32v( 0xAAAAAAAB ); // [0,3) in the highest two bits, [0,8) in the lowest three bits // ( -4LL << 30 ) / 3
        int32v indexPermutation2HighBit = ( indexFacetBasisWithPermute2 << 29 ); // & int32v( 1 << 31 ); // [0,1) in the most significant bit
        int32v indexPermutation3 = FS::BitShiftRightZeroExtend( hashShifted * int32v( 3 ), 30 ); // [0,3)
        float32v finalSign = FS::Cast<float>( hash31 << 31 );

        float32v valueAB, valueBA, valueC;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexFacetBasisWithPermute2 = FS::NativeExec<int32v>( []( auto a ){ return _mm512_rol_epi32( a, 2 ); }, indexFacetBasisWithPermute2 );

            const auto tableA_gX = FS::Constant<float>( kComponentA, kComponentA, kComponentC, kComponentC, -kComponentA, -kComponentA, kComponentC, kComponentC, kComponentA, kComponentA, kComponentC, kComponentC, -kComponentA, -kComponentA, kComponentC, kComponentC );
            const auto tableA_gY = FS::Constant<float>( kComponentC, kComponentB, kComponentA, kComponentA, kComponentC, kComponentB, -kComponentA, -kComponentA, kComponentC, -kComponentB, kComponentA, kComponentA, kComponentC, -kComponentB, -kComponentA, -kComponentA );
            const auto tableA_gZ = FS::Constant<float>( kComponentB, kComponentC, kComponentB, kComponentB, kComponentB, kComponentC, kComponentB, kComponentB, -kComponentB, kComponentC, -kComponentB, -kComponentB, -kComponentB, kComponentC, -kComponentB, -kComponentB );

            const auto tableB_gX = FS::Constant<float>( kComponentB, kComponentB, kComponentC, kComponentC, -kComponentB, -kComponentB, kComponentC, kComponentC, kComponentB, kComponentB, kComponentC, kComponentC, -kComponentB, -kComponentB, kComponentC, kComponentC );
            const auto tableB_gY = FS::Constant<float>( kComponentC, kComponentA, kComponentB, kComponentB, kComponentC, kComponentA, -kComponentB, -kComponentB, kComponentC, -kComponentA, kComponentB, kComponentB, kComponentC, -kComponentA, -kComponentB, -kComponentB );
            const auto tableB_gZ = FS::Constant<float>( kComponentA, kComponentC, kComponentA, kComponentA, kComponentA, kComponentC, kComponentA, kComponentA, -kComponentA, kComponentC, -kComponentA, -kComponentA, -kComponentA, kComponentC, -kComponentA, -kComponentA );

            const auto tableC_gX = FS::Constant<float>( kComponentsDE, kComponentsDE, kComponentF, kComponentF, kComponentC, kComponentC, kComponentF, kComponentF, kComponentsDE, kComponentsDE, kComponentF, kComponentF, kComponentC, kComponentC, kComponentF, kComponentF );
            const auto tableC_gY = FS::Constant<float>( kComponentF, kComponentsDE, kComponentsDE, kComponentsDE, kComponentF, kComponentsDE, kComponentC, kComponentC, kComponentF, kComponentC, kComponentsDE, kComponentsDE, kComponentF, kComponentC, kComponentC, kComponentC );
            const auto tableC_gZ = FS::Constant<float>( kComponentsDE, kComponentF, kComponentsDE, kComponentsDE, kComponentsDE, kComponentF, kComponentsDE, kComponentsDE, kComponentC, kComponentF, kComponentC, kComponentC, kComponentC, kComponentF, kComponentC, kComponentC );

            float32v valueAB_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gX, indexFacetBasisWithPermute2, tableB_gX );
            float32v valueAB_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gY, indexFacetBasisWithPermute2, tableB_gY );
            float32v valueAB_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gZ, indexFacetBasisWithPermute2, tableB_gZ );
            valueAB = FS::FMulAdd( valueAB_gZ, fZ, FS::FMulAdd( fY, valueAB_gY, fX * valueAB_gX ) );

            float32v valueBA_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gX, indexFacetBasisWithPermute2, tableA_gX );
            float32v valueBA_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gY, indexFacetBasisWithPermute2, tableA_gY );
            float32v valueBA_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gZ, indexFacetBasisWithPermute2, tableA_gZ );
            valueBA = FS::FMulAdd( valueBA_gZ, fZ, FS::FMulAdd( fY, valueBA_gY, fX * valueBA_gX ) );

            float32v valueC_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gX );
            float32v valueC_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gY );
            float32v valueC_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gZ );
            valueC = FS::FMulAdd( valueC_gZ, fZ, FS::FMulAdd( fY, valueC_gY, fX * valueC_gX ) );
        }
        else
        {
            float32v sign0 = FS::Cast<float>( indexFacetBasisWithPermute2 << 31 );
            float32v sign1 = FS::Cast<float>( ( indexFacetBasisWithPermute2 << 30 ) & int32v( 1 << 31 ) );

            auto notYZ = indexFacetBasisWithPermute2;
            auto notXY = indexFacetBasisWithPermute2 << 1;

            float32v valueA_gX = FS::SelectHighBit( notYZ, float32v( kComponentC ), float32v( kComponentA ) ^ sign0 );
            float32v valueA_gY = FS::SelectHighBit( notYZ | notXY, FS::SelectHighBit( notXY, float32v( kComponentB ) ^ sign1, float32v( kComponentA ) ^ sign0 ), float32v( kComponentC ) );
            float32v valueA_gZ = FS::SelectHighBit( notXY, float32v( kComponentC ), float32v( kComponentB ) ^ sign1 );
            float32v valueA = FS::FMulAdd( valueA_gZ, fZ, FS::FMulAdd( fY, valueA_gY, fX * valueA_gX ) );

            float32v valueB_gX = FS::SelectHighBit( notYZ, float32v( kComponentC ), float32v( kComponentB ) ^ sign0 );
            float32v valueB_gY = FS::SelectHighBit( notYZ | notXY, FS::SelectHighBit( notXY, float32v( kComponentA ) ^ sign1, float32v( kComponentB ) ^ sign0 ), float32v( kComponentC ) );
            float32v valueB_gZ = FS::SelectHighBit( notXY, float32v( kComponentC ), float32v( kComponentA ) ^ sign1 );
            float32v valueB = FS::FMulAdd( valueB_gZ, fZ, FS::FMulAdd( fY, valueB_gY, fX * valueB_gX ) );

            float32v valueC_gX = FS::SelectHighBit( notYZ, float32v( kComponentF ), float32v( kComponentsDE ) ^ sign0 );
            float32v valueC_gY = FS::SelectHighBit( notYZ | notXY, FS::SelectHighBit( notXY, float32v( kComponentsDE ) ^ sign1, float32v( kComponentsDE ) ^ sign0 ), float32v( kComponentF ) );
            float32v valueC_gZ = FS::SelectHighBit( notXY, float32v( kComponentF ), float32v( kComponentsDE ) ^ sign1 );
            valueC = FS::FMulAdd( valueC_gZ, fZ, FS::FMulAdd( fY, valueC_gY, fX * valueC_gX ) );

            valueAB = FS::SelectHighBit( indexPermutation2HighBit, valueB, valueA );
            valueBA = FS::SelectHighBit( indexPermutation2HighBit, valueA, valueB );
        }
        
        multiplier ^= finalSign;
        valueX = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == int32v( 0 ), valueC, valueAB ), valueX );
        valueY = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == int32v( 1 ), valueC, FS::Select( indexPermutation3 == int32v( 2 ), valueBA, valueAB ) ), valueY );
        valueZ = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == int32v( 2 ), valueC, valueBA ), valueZ );
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductSimplex( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW )
    {
        constexpr float kComponentPairwiseIndexedNegativeAB = -0.375999676691291f;
        constexpr float kComponentPairwiseUnindexedFillerAB = 0.222726847849776f;
        constexpr float kComponentPairwiseIndexedPositiveD = -kSkew4f;
        constexpr float kComponentPairwiseUnindexedD = kSkew4f;

        constexpr float kDeltaPairwiseToSingleAB = -0.124000323308709f;
        constexpr float kDeltaPairwiseToSingleD = 0.190983005625053f;
        constexpr float kDeltaSingleToExtra = kSkew4f;
        constexpr float kDeltaPairwiseABToC = 0.437016024448821f;
        constexpr float kDeltaUnindexedFillerToDiagonal = -kRoot2f;

        constexpr float kDeltaPairwiseToSingleExtraAB = kDeltaPairwiseToSingleAB + kDeltaSingleToExtra;
        constexpr float kDeltaPairwiseToSingleExtraD = kDeltaPairwiseToSingleD + kDeltaSingleToExtra;

        constexpr float sIdxABC = kComponentPairwiseIndexedNegativeAB + kDeltaPairwiseToSingleAB;
        constexpr float sDiagABC = kComponentPairwiseUnindexedFillerAB + kDeltaPairwiseToSingleAB + kDeltaUnindexedFillerToDiagonal;
        constexpr float sFillABC = kComponentPairwiseUnindexedFillerAB + kDeltaPairwiseToSingleAB;
        constexpr float sIdxD = kComponentPairwiseIndexedPositiveD + kDeltaPairwiseToSingleD - 1;
        constexpr float sFillD = kComponentPairwiseUnindexedD + kDeltaPairwiseToSingleD;

        constexpr float pIdxPosAB = kComponentPairwiseIndexedNegativeAB + 1;
        constexpr float pIdxNegAB = kComponentPairwiseIndexedNegativeAB;
        constexpr float pFillAB = kComponentPairwiseUnindexedFillerAB;
        constexpr float pDiagAB = kComponentPairwiseUnindexedFillerAB + kDeltaUnindexedFillerToDiagonal;
        constexpr float pIdxPosC = kComponentPairwiseIndexedNegativeAB + kDeltaPairwiseABToC + 1;
        constexpr float pIdxNegC = kComponentPairwiseIndexedNegativeAB + kDeltaPairwiseABToC;
        constexpr float pFillC = kComponentPairwiseUnindexedFillerAB + kDeltaPairwiseABToC;
        constexpr float pIdxPosD = kComponentPairwiseIndexedPositiveD;
        constexpr float pIdxNegD = kComponentPairwiseIndexedPositiveD - 1;
        constexpr float pFillD = kComponentPairwiseUnindexedD;

        constexpr float eIdxABC = kComponentPairwiseIndexedNegativeAB + kDeltaPairwiseToSingleExtraAB + 1;
        constexpr float eDiagABC = kComponentPairwiseUnindexedFillerAB + kDeltaPairwiseToSingleExtraAB + kDeltaUnindexedFillerToDiagonal;
        constexpr float eFillABC = kComponentPairwiseUnindexedFillerAB + kDeltaPairwiseToSingleExtraAB;
        constexpr float eIdxD = kComponentPairwiseIndexedPositiveD + kDeltaPairwiseToSingleExtraD;
        constexpr float eFillD = kComponentPairwiseUnindexedD + kDeltaPairwiseToSingleExtraD;

        int32v hashShifted = FS::BitShiftRightZeroExtend( hash31, 2 );
        int32v indexBasis = hashShifted * int32v( 20 >> 2 ); // [0,20) << 27
        int32v indexPermutation3 = ( hashShifted * int32v( 0xD5555556 ) ) >> 29; // [0,3] // ( -4LL << 29 ) / 3
        int32v indexPermutation8 = indexBasis >> 24; // & int32v( 0x07 );
        float32v finalSign = FS::Cast<float>( hash31 << 31 );

        float32v valueA, valueB, valueC, valueD;
        float32v valueA_gX, valueB_gX, valueC_gX, valueD_gX;
        float32v valueA_gY, valueB_gY, valueC_gY, valueD_gY;
        float32v valueA_gZ, valueB_gZ, valueC_gZ, valueD_gZ;
        float32v valueA_gW, valueB_gW, valueC_gW, valueD_gW;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexBasis >>= 27;

            valueA_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sDiagABC, sDiagABC, sDiagABC, pIdxPosAB, pIdxNegAB, pDiagAB, pDiagAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pDiagAB, pDiagAB, pIdxNegAB ), indexBasis,
                FS::Constant<float>( eIdxABC, eDiagABC, eDiagABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sFillABC, sFillABC, sFillABC, pIdxPosAB, pIdxNegAB, pFillAB, pFillAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pFillAB, pFillAB, pIdxNegAB ), indexBasis,
                FS::Constant<float>( eIdxABC, eFillABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sFillABC, sFillABC, sFillABC, pIdxPosC, pIdxNegC, pFillC, pFillC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pFillC, pFillC, pIdxNegC ), indexBasis,
                FS::Constant<float>( eIdxABC, eFillABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxD, sFillD, sFillD, sFillD, pIdxPosD, pIdxNegD, pFillD, pFillD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pFillD, pFillD, pIdxNegD ), indexBasis,
                FS::Constant<float>( eIdxD, eFillD, eFillD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = valueA_gX * fX;
            valueB = valueB_gX * fX;
            valueC = valueC_gX * fX;
            valueD = valueD_gX * fX;

            valueA_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sIdxABC, sFillABC, sFillABC, pIdxNegAB, pIdxPosAB, pFillAB, pFillAB, pDiagAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pIdxNegAB, pDiagAB ), indexBasis,
                FS::Constant<float>( eDiagABC, eIdxABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sIdxABC, sDiagABC, sDiagABC, pIdxNegAB, pIdxPosAB, pDiagAB, pDiagAB, pFillAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pIdxNegAB, pFillAB ), indexBasis,
                FS::Constant<float>( eFillABC, eIdxABC, eDiagABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sIdxABC, sFillABC, sFillABC, pIdxNegC, pIdxPosC, pFillC, pFillC, pFillC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pIdxNegC, pFillC ), indexBasis,
                FS::Constant<float>( eFillABC, eIdxABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sIdxD, sFillD, sFillD, pIdxNegD, pIdxPosD, pFillD, pFillD, pFillD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pIdxNegD, pFillD ), indexBasis,
                FS::Constant<float>( eFillD, eIdxD, eFillD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gY, fY, valueA );
            valueB = FS::FMulAdd( valueB_gY, fY, valueB );
            valueC = FS::FMulAdd( valueC_gY, fY, valueC );
            valueD = FS::FMulAdd( valueD_gY, fY, valueD );

            valueA_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sIdxABC, sFillABC, pDiagAB, pDiagAB, pIdxPosAB, pIdxNegAB, pIdxNegAB, pFillAB, pIdxPosAB, pFillAB, pFillAB, pIdxNegAB, pIdxPosAB, pFillAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eIdxABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sDiagABC, sIdxABC, sFillABC, pFillAB, pFillAB, pIdxPosAB, pIdxNegAB, pIdxNegAB, pDiagAB, pIdxPosAB, pDiagAB, pDiagAB, pIdxNegAB, pIdxPosAB, pDiagAB ), indexBasis,
                FS::Constant<float>( eDiagABC, eDiagABC, eIdxABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sIdxABC, sDiagABC, pFillC, pFillC, pIdxPosC, pIdxNegC, pIdxNegC, pFillC, pIdxPosC, pFillC, pFillC, pIdxNegC, pIdxPosC, pFillC ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eIdxABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sFillD, sIdxD, sFillD, pFillD, pFillD, pIdxPosD, pIdxNegD, pIdxNegD, pFillD, pIdxPosD, pFillD, pFillD, pIdxNegD, pIdxPosD, pFillD ), indexBasis,
                FS::Constant<float>( eFillD, eFillD, eIdxD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gZ, fZ, valueA );
            valueB = FS::FMulAdd( valueB_gZ, fZ, valueB );
            valueC = FS::FMulAdd( valueC_gZ, fZ, valueC );
            valueD = FS::FMulAdd( valueD_gZ, fZ, valueD );

            valueA_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sFillABC, sIdxABC, pFillAB, pFillAB, pIdxNegAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pIdxNegAB, pFillAB, pFillAB, pIdxPosAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eFillABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sFillABC, sIdxABC, pDiagAB, pDiagAB, pIdxNegAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pIdxNegAB, pDiagAB, pDiagAB, pIdxPosAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eFillABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sDiagABC, sDiagABC, sIdxABC, pFillC, pFillC, pIdxNegC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pIdxNegC, pFillC, pFillC, pIdxPosC ), indexBasis,
                FS::Constant<float>( eDiagABC, eDiagABC, eDiagABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sFillD, sFillD, sIdxD, pFillD, pFillD, pIdxNegD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pIdxNegD, pFillD, pFillD, pIdxPosD ), indexBasis,
                FS::Constant<float>( eFillD, eFillD, eFillD, eIdxD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gW, fW, valueA );
            valueB = FS::FMulAdd( valueB_gW, fW, valueB );
            valueC = FS::FMulAdd( valueC_gW, fW, valueC );
            valueD = FS::FMulAdd( valueD_gW, fW, valueD );
        }
        else if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
        {
            const auto tableAB = FS::Constant<float>( pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, sFillABC, sIdxABC, eFillABC, eIdxABC );
            const auto tableC  = FS::Constant<float>( pFillC,  pIdxNegC,  pFillC,  pIdxPosC,  sFillABC, sIdxABC, eFillABC, eIdxABC );
            const auto tableD  = FS::Constant<float>( pFillD,  pIdxNegD,  pFillD,  pIdxPosD,  sFillD,   sIdxD,   eFillD,   eIdxD   );

            int32v indexPositive = indexBasis & int32v( 0x03 << 27 );
            int32v indexNegative = ( indexBasis >> 2 ) & int32v( 0x03 << 27 );
            indexNegative ^= indexPositive;

            auto extraCase = ( indexBasis >= int32v( 0x10 << 27 ) );
            auto singleCase = ( indexPositive == indexNegative );
            indexPositive |= FS::Cast<int32_t>( singleCase ); // Force indexPositive checks to fail

            int32v indexSelectBase = FS::Masked( singleCase, int32v( 4 ) ) | FS::Masked( extraCase, int32v( 2 ) );

            int32v indexedCounter( -1 );

            {
                auto indexedPositive = ( indexPositive == int32v( 0 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 0 << 27 ) );
                int32v indexSelect = FS::MaskedIncrement( indexed, indexSelectBase | FS::Masked( indexedPositive, int32v( 2 ) ) );

                valueA_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, sDiagABC, sIdxABC, eDiagABC, eIdxABC ), indexSelect );
                valueB_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableAB, indexSelect );
                valueC_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableC,  indexSelect );
                valueD_gX = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableD,  indexSelect );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = valueA_gX * fX;
            valueB = valueB_gX * fX;
            valueC = valueC_gX * fX;
            valueD = valueD_gX * fX;

            {
                auto indexedPositive = ( indexPositive == int32v( 1 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 1 << 27 ) );
                int32v indexSelect = FS::MaskedIncrement( indexed, indexSelectBase | FS::Masked( indexedPositive, int32v( 2 ) ) );

                valueA_gY = valueB_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableAB, indexSelect );
                valueC_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableC, indexSelect );
                valueD_gY = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableD, indexSelect );

                int32v maskedIndexedCounter = FS::InvMasked( indexed, indexedCounter );
                valueA_gY = FS::MaskedAdd( maskedIndexedCounter == int32v( -2 ), valueA_gY, float32v( kDeltaUnindexedFillerToDiagonal ) );
                valueB_gY = FS::MaskedAdd( maskedIndexedCounter == int32v( -1 ), valueB_gY, float32v( kDeltaUnindexedFillerToDiagonal ) );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = FS::FMulAdd( valueA_gY, fY, valueA );
            valueB = FS::FMulAdd( valueB_gY, fY, valueB );
            valueC = FS::FMulAdd( valueC_gY, fY, valueC );
            valueD = FS::FMulAdd( valueD_gY, fY, valueD );

            {
                auto indexedPositive = ( indexPositive == int32v( 2 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 2 << 27 ) );
                int32v indexSelect = FS::MaskedIncrement( indexed, indexSelectBase | FS::Masked( indexedPositive, int32v( 2 ) ) );

                valueA_gZ = valueB_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableAB, indexSelect );
                valueC_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableC, indexSelect );
                valueD_gZ = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableD, indexSelect );

                int32v maskedIndexedCounter = FS::InvMasked( indexed, indexedCounter );
                valueA_gZ = FS::MaskedAdd( maskedIndexedCounter == int32v( -3 ), valueA_gZ, float32v( kDeltaUnindexedFillerToDiagonal ) );
                valueB_gZ = FS::MaskedAdd( maskedIndexedCounter == int32v( -2 ), valueB_gZ, float32v( kDeltaUnindexedFillerToDiagonal ) );
                valueC_gZ = FS::MaskedAdd( maskedIndexedCounter == int32v( -1 ), valueC_gZ, float32v( kDeltaUnindexedFillerToDiagonal ) );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = FS::FMulAdd( valueA_gZ, fZ, valueA );
            valueB = FS::FMulAdd( valueB_gZ, fZ, valueB );
            valueC = FS::FMulAdd( valueC_gZ, fZ, valueC );
            valueD = FS::FMulAdd( valueD_gZ, fZ, valueD );

            {
                auto indexedPositive = ( indexPositive == int32v( 3 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 3 << 27 ) );
                int32v indexSelect = FS::MaskedIncrement( indexed, indexSelectBase | FS::Masked( indexedPositive, int32v( 2 ) ) );

                valueA_gW = valueB_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableAB, indexSelect );
                valueC_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableC, indexSelect );
                valueD_gW = FS::NativeExec<float32v>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), tableD, indexSelect );

                int32v maskedIndexedCounter = FS::InvMasked( indexed, indexedCounter );
                valueB_gW = FS::MaskedAdd( maskedIndexedCounter == int32v( -3 ), valueB_gW, float32v( kDeltaUnindexedFillerToDiagonal ) );
                valueC_gW = FS::MaskedAdd( maskedIndexedCounter == int32v( -2 ), valueC_gW, float32v( kDeltaUnindexedFillerToDiagonal ) );
            }

            valueA = FS::FMulAdd( valueA_gW, fW, valueA );
            valueB = FS::FMulAdd( valueB_gW, fW, valueB );
            valueC = FS::FMulAdd( valueC_gW, fW, valueC );
            valueD = FS::FMulAdd( valueD_gW, fW, valueD );
        }
        else
        {
            int32v indexPositive = indexBasis & int32v( 0x03 << 27 );
            int32v indexNegative = ( indexBasis >> 2 ) & int32v( 0x03 << 27 );
            indexNegative ^= indexPositive;

            auto extraCase = ( indexBasis >= int32v( 0x10 << 27 ) );
            auto singleCase = ( indexPositive == indexNegative );
            auto singleNonExtraCase = indexBasis < int32v( 0x04 << 27 );
            indexPositive |= FS::Cast<int32_t>( singleNonExtraCase ); // Force indexPositive checks to fail

            float32v singleOffsetAB = FS::MaskedAdd( extraCase, float32v( kDeltaPairwiseToSingleAB ), float32v( kDeltaSingleToExtra ) );
            float32v componentIndexedNegativeAB = FS::MaskedAdd( singleCase, float32v( kComponentPairwiseIndexedNegativeAB ), singleOffsetAB );
            float32v componentUnindexedFillerAB = FS::MaskedAdd( singleCase, float32v( kComponentPairwiseUnindexedFillerAB ), singleOffsetAB );

            float32v componentIndexedNegativeC = FS::InvMaskedAdd( singleCase, componentIndexedNegativeAB, float32v( kDeltaPairwiseABToC ) );
            float32v componentUnindexedFillerC = FS::InvMaskedAdd( singleCase, componentUnindexedFillerAB, float32v( kDeltaPairwiseABToC ) );

            float32v singleOffsetD = FS::MaskedAdd( extraCase, float32v( kDeltaPairwiseToSingleD ), float32v( kDeltaSingleToExtra ) );
            float32v componentIndexedPositiveD = FS::MaskedAdd( singleCase, float32v( kComponentPairwiseIndexedPositiveD ), singleOffsetD );
            float32v componentUnindexedD = FS::MaskedAdd( singleCase, float32v( kComponentPairwiseUnindexedD ), singleOffsetD );

            int32v indexedCounter( -1 );

            {
                auto indexedPositive = ( indexPositive == int32v( 0 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 0 << 27 ) );

                float32v indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                float32v indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                float32v indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                float32v unindexedComponentA = componentUnindexedFillerAB + float32v( kDeltaUnindexedFillerToDiagonal );
                float32v unindexedComponentB = componentUnindexedFillerAB;
                float32v unindexedComponentC = componentUnindexedFillerC;

                valueA_gX = FS::Select( indexed, indexedComponentAB, unindexedComponentA );
                valueB_gX = FS::Select( indexed, indexedComponentAB, unindexedComponentB );
                valueC_gX = FS::Select( indexed, indexedComponentC,  unindexedComponentC );
                valueD_gX = FS::Select( indexed, indexedComponentD,  componentUnindexedD );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = valueA_gX * fX;
            valueB = valueB_gX * fX;
            valueC = valueC_gX * fX;
            valueD = valueD_gX * fX;

            {
                auto indexedPositive = ( indexPositive == int32v( 1 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 1 << 27 ) );

                float32v indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                float32v indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                float32v indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                float32v unindexedComponentA = FS::MaskedAdd( indexedCounter == int32v( -2 ), componentUnindexedFillerAB, float32v( kDeltaUnindexedFillerToDiagonal ) );
                float32v unindexedComponentB = FS::MaskedAdd( indexedCounter == int32v( -1 ), componentUnindexedFillerAB, float32v( kDeltaUnindexedFillerToDiagonal ) );
                float32v unindexedComponentC = componentUnindexedFillerC;

                valueA_gY = FS::Select( indexed, indexedComponentAB, unindexedComponentA );
                valueB_gY = FS::Select( indexed, indexedComponentAB, unindexedComponentB );
                valueC_gY = FS::Select( indexed, indexedComponentC,  unindexedComponentC );
                valueD_gY = FS::Select( indexed, indexedComponentD,  componentUnindexedD );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = FS::FMulAdd( valueA_gY, fY, valueA );
            valueB = FS::FMulAdd( valueB_gY, fY, valueB );
            valueC = FS::FMulAdd( valueC_gY, fY, valueC );
            valueD = FS::FMulAdd( valueD_gY, fY, valueD );

            {
                auto indexedPositive = ( indexPositive == int32v( 2 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 2 << 27 ) );

                float32v indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                float32v indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                float32v indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                float32v unindexedComponentA = FS::MaskedAdd( indexedCounter == int32v( -3 ), componentUnindexedFillerAB, float32v( kDeltaUnindexedFillerToDiagonal ) );
                float32v unindexedComponentB = FS::MaskedAdd( indexedCounter == int32v( -2 ), componentUnindexedFillerAB, float32v( kDeltaUnindexedFillerToDiagonal ) );
                float32v unindexedComponentC = FS::MaskedAdd( indexedCounter == int32v( -1 ), componentUnindexedFillerC,  float32v( kDeltaUnindexedFillerToDiagonal ) );

                valueA_gZ = FS::Select( indexed, indexedComponentAB, unindexedComponentA );
                valueB_gZ = FS::Select( indexed, indexedComponentAB, unindexedComponentB );
                valueC_gZ = FS::Select( indexed, indexedComponentC,  unindexedComponentC );
                valueD_gZ = FS::Select( indexed, indexedComponentD,  componentUnindexedD );

                indexedCounter = FS::MaskedDecrement( indexed, indexedCounter );
            }

            valueA = FS::FMulAdd( valueA_gZ, fZ, valueA );
            valueB = FS::FMulAdd( valueB_gZ, fZ, valueB );
            valueC = FS::FMulAdd( valueC_gZ, fZ, valueC );
            valueD = FS::FMulAdd( valueD_gZ, fZ, valueD );

            {
                auto indexedPositive = ( indexPositive == int32v( 3 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == int32v( 3 << 27 ) );

                float32v indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                float32v indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                float32v indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                float32v unindexedComponentA = componentUnindexedFillerAB;
                float32v unindexedComponentB = FS::MaskedAdd( indexedCounter == int32v( -3 ), componentUnindexedFillerAB, float32v( kDeltaUnindexedFillerToDiagonal ) );
                float32v unindexedComponentC = FS::MaskedAdd( indexedCounter == int32v( -2 ), componentUnindexedFillerC,  float32v( kDeltaUnindexedFillerToDiagonal ) );

                valueA_gW = FS::Select( indexed, indexedComponentAB, unindexedComponentA );
                valueB_gW = FS::Select( indexed, indexedComponentAB, unindexedComponentB );
                valueC_gW = FS::Select( indexed, indexedComponentC,  unindexedComponentC );
                valueD_gW = FS::Select( indexed, indexedComponentD,  componentUnindexedD );
            }

            valueA = FS::FMulAdd( valueA_gW, fW, valueA );
            valueB = FS::FMulAdd( valueB_gW, fW, valueB );
            valueC = FS::FMulAdd( valueC_gW, fW, valueC );
            valueD = FS::FMulAdd( valueD_gW, fW, valueD );
        }

        int32v valueIndexX = ( indexPermutation8 >> 1 ); // & int32v( 0x3 );
        int32v valueIndexY = ( FS::Increment( valueIndexX ) + indexPermutation3 ); // & int32v( 0x3 );
        int32v valueIndexZ = indexPermutation8 & int32v( 0x1 );
        valueIndexZ = ( FS::Increment( valueIndexX ) + FS::MaskedIncrement( valueIndexZ >= indexPermutation3, valueIndexZ ) ); // & int32v( 0x3 );
        int32v valueIndexSumXYZ = valueIndexX + valueIndexY + valueIndexZ;

        multiplier ^= finalSign;
        valueX = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexX << 31, FS::SelectHighBit( valueIndexX << 30, valueD, valueB ), FS::SelectHighBit( valueIndexX << 30, valueC, valueA ) ), valueX );
        valueY = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexY << 31, FS::SelectHighBit( valueIndexY << 30, valueD, valueB ), FS::SelectHighBit( valueIndexY << 30, valueC, valueA ) ), valueY );
        valueZ = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexZ << 31, FS::SelectHighBit( valueIndexZ << 30, valueD, valueB ), FS::SelectHighBit( valueIndexZ << 30, valueC, valueA ) ), valueZ );
        valueW = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexSumXYZ << 31, FS::SelectHighBit( valueIndexSumXYZ << 30, valueD, valueB ), FS::SelectHighBit( valueIndexSumXYZ << 30, valueA, valueC ) ), valueW );
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionSimplex( int32v hash, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalGradientMatrixVectorProductSimplex( hash, fX, fY, multiplier, valueX, valueY );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductSimplex( hash, fX, fY, multiplier, valueX, valueY );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionCommon( int32v hash, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalGradientMatrixVectorProductCommon( hash, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductCommon( hash, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE static void ApplyVectorContributionSimplex( int32v hash, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW ) {
        switch( Scheme ) {
            case VectorizationScheme::OrthogonalGradientMatrix:
                return ApplyOrthogonalGradientMatrixVectorProductSimplex( hash, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
            case VectorizationScheme::GradientOuterProduct:
                return ApplyGradientOuterProductVectorProductSimplex( hash, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
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

            return FS::Masked( invSqrt != float32v( kInfinity ), sqrDist * invSqrt );
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
