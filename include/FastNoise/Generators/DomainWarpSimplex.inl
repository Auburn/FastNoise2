#include "DomainWarpSimplex.h"
#include "Utils.inl"

namespace FastNoise
{
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
}

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpSimplex, SIMD> final : public virtual DomainWarpSimplex, public DispatchClass<DomainWarp, SIMD>
{
public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_2D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, xOut, yOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_2D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, xOut, yOut );
        }
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme ) 
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_3D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_3D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        }        
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        seed += int32v( mSeedOffset );
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_4D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_4D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
    }

protected:
    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        constexpr double kRoot3 = 1.7320508075688772935274463415059;
        constexpr double kSkew2 = 1.0 / ( kRoot3 + 1.0 );
        constexpr double kUnskew2 = -1.0 / ( kRoot3 + 3.0 );
        constexpr double kFalloffRadiusSquared = 0.5;

        float32v skewDelta = float32v( kSkew2 ) * ( x + y );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );

        mask32v xGreaterEqualY = dxSkewed >= dySkewed;

        float32v unskewDelta = float32v( kUnskew2 ) * ( dxSkewed + dySkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;

        float32v dx1 = FS::MaskedIncrement( ~xGreaterEqualY, dx0 ) - float32v( kUnskew2 + 1 );
        float32v dy1 = FS::MaskedIncrement( xGreaterEqualY, dy0 ) - float32v( kUnskew2 + 1 );
        float32v dx2 = dx0 - float32v( kUnskew2 * 2 + 1 );
        float32v dy2 = dy0 - float32v( kUnskew2 * 2 + 1 );

        float32v falloff0 = FS::FNMulAdd( dx0, dx0, FS::FNMulAdd( dy0, dy0, float32v( kFalloffRadiusSquared ) ) );
        float32v falloff1 = FS::FNMulAdd( dx1, dx1, FS::FNMulAdd( dy1, dy1, float32v( kFalloffRadiusSquared ) ) );
        float32v falloff2 = falloff0 + FS::FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot3 + 2.0 ) / ( kRoot3 + 3.0 ) ),
            float32v( -2.0 / 3.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;

        float32v valueX( 0 );
        float32v valueY( 0 );

        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase ), dx0, dy0, falloff0, valueX, valueY );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, FS::MaskedAdd( xGreaterEqualY, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( xGreaterEqualY, yPrimedBase, int32v( Primes::Y ) ) ), dx1, dy1, falloff1, valueX, valueY );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) ), dx2, dy2, falloff2, valueX, valueY );

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(49.918426513671875 / 2.0) :
            70.1480577066486f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );

        return FS::FMulAdd( valueY, valueY, valueX * valueX );
    }

    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v skewDelta = float32v( kSkew3 ) * ( x + y + z );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );

        mask32v xGreaterEqualY = dxSkewed >= dySkewed;
        mask32v yGreaterEqualZ = dySkewed >= dzSkewed;
        mask32v xGreaterEqualZ = dxSkewed >= dzSkewed;

        float32v unskewDelta = float32v( kReflectUnskew3 ) * ( dxSkewed + dySkewed + dzSkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;
        float32v dz0 = dzSkewed + unskewDelta;

        mask32v maskX1 = xGreaterEqualY & xGreaterEqualZ;
        mask32v maskY1 = FS::BitwiseAndNot( yGreaterEqualZ, xGreaterEqualY );
        mask32v maskZ1 = xGreaterEqualZ | yGreaterEqualZ; // Inv masked

        mask32v nMaskX2 = xGreaterEqualY | xGreaterEqualZ; // Inv masked
        mask32v nMaskY2 = FS::BitwiseAndNot( xGreaterEqualY, yGreaterEqualZ );
        mask32v nMaskZ2 = xGreaterEqualZ & yGreaterEqualZ;

        float32v dx3 = dx0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dy3 = dy0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dz3 = dz0 - float32v( kReflectUnskew3 * 3 + 1 );
        float32v dx1 = FS::MaskedSub( maskX1, dx3, float32v( 1 ) ); // kReflectUnskew3 * 3 + 1 = kReflectUnskew3, so dx0 - kReflectUnskew3 = dx3
        float32v dy1 = FS::MaskedSub( maskY1, dy3, float32v( 1 ) );
        float32v dz1 = FS::InvMaskedSub( maskZ1, dz3, float32v( 1 ) );
        float32v dx2 = FS::MaskedIncrement( ~nMaskX2, dx0 ); // kReflectUnskew3 * 2 - 1 = 0, so dx0 + ( kReflectUnskew3 * 2 - 1 ) = dx0
        float32v dy2 = FS::MaskedIncrement( nMaskY2, dy0 );
        float32v dz2 = FS::MaskedIncrement( nMaskZ2, dz0 );

        float32v falloff0 = FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff1 = FS::FNMulAdd( dz1, dz1, FS::FNMulAdd( dy1, dy1, FS::FNMulAdd( dx1, dx1, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff2 = FS::FNMulAdd( dz2, dz2, FS::FNMulAdd( dy2, dy2, FS::FNMulAdd( dx2, dx2, float32v( kFalloffRadiusSquared ) ) ) );
        float32v falloff3 = falloff0 - ( unskewDelta + float32v( 3.0 / 4.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );
        falloff3 = FS::Max( falloff3, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;
        falloff3 *= falloff3; falloff3 *= falloff3;

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );

        ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dx0, dy0, dz0, falloff0, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ), FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ) ), dx1, dy1, dz1, falloff1, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, FS::MaskedAdd( nMaskX2, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( nMaskY2, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( nMaskZ2, zPrimedBase, int32v( Primes::Z ) ) ), dx2, dy2, dz2, falloff2, valueX, valueY, valueZ );
        ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ) ), dx3, dy3, dz3, falloff3, valueX, valueY, valueZ );

        if constexpr( Scheme != VectorizationScheme::OrthogonalGradientMatrix )
        {
            // Match gradient orientation.
            constexpr double kReflect3D = -2.0 / 2.0;
            float32v valueTransformDelta = float32v( kReflect3D ) * ( valueX + valueY + valueZ );
            valueX += valueTransformDelta;
            valueY += valueTransformDelta;
            valueZ += valueTransformDelta;
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(32.69428253173828125 / 1.4142135623730951) :
            16.281631889139874f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        return FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) );
    }

    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v skewDelta = float32v( kSkew4 ) * ( x + y + z + w );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v wSkewed = w + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v wSkewedBase = FS::Floor( wSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;
        float32v dwSkewed = wSkewed - wSkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );
        int32v wPrimedBase = FS::Convert<int32_t>( wSkewedBase ) * int32v( Primes::W );

        float32v unskewDelta = float32v( kUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;
        float32v dz0 = dzSkewed + unskewDelta;
        float32v dw0 = dwSkewed + unskewDelta;

        int32v rankX( 0 );
        int32v rankY( 0 );
        int32v rankZ( 0 );
        int32v rankW( 0 );

        mask32v xGreaterEqualY = dx0 >= dy0;
        rankX = FS::MaskedIncrement( xGreaterEqualY, rankX );
        rankY = FS::MaskedIncrement( ~xGreaterEqualY, rankY );

        mask32v xGreaterEqualZ = dx0 >= dz0;
        rankX = FS::MaskedIncrement( xGreaterEqualZ, rankX );
        rankZ = FS::MaskedIncrement( ~xGreaterEqualZ, rankZ );

        mask32v xGreaterEqualW = dx0 >= dw0;
        rankX = FS::MaskedIncrement( xGreaterEqualW, rankX );
        rankW = FS::MaskedIncrement( ~xGreaterEqualW, rankW );

        mask32v yGreaterEqualZ = dy0 >= dz0;
        rankY = FS::MaskedIncrement( yGreaterEqualZ, rankY );
        rankZ = FS::MaskedIncrement( ~yGreaterEqualZ, rankZ );

        mask32v yGreaterEqualW = dy0 >= dw0;
        rankY = FS::MaskedIncrement( yGreaterEqualW, rankY );
        rankW = FS::MaskedIncrement( ~yGreaterEqualW, rankW );

        mask32v zGreaterEqualW = dz0 >= dw0;
        rankZ = FS::MaskedIncrement( zGreaterEqualW, rankZ );
        rankW = FS::MaskedIncrement( ~zGreaterEqualW, rankW );

        mask32v maskX1 = rankX > int32v( 2 );
        mask32v maskY1 = rankY > int32v( 2 );
        mask32v maskZ1 = rankZ > int32v( 2 );
        mask32v maskW1 = rankW > int32v( 2 );

        mask32v maskX2 = rankX > int32v( 1 );
        mask32v maskY2 = rankY > int32v( 1 );
        mask32v maskZ2 = rankZ > int32v( 1 );
        mask32v maskW2 = rankW > int32v( 1 );

        mask32v maskX3 = rankX > int32v( 0 );
        mask32v maskY3 = rankY > int32v( 0 );
        mask32v maskZ3 = rankZ > int32v( 0 );
        mask32v maskW3 = rankW > int32v( 0 );

        float32v dx1 = FS::MaskedSub( maskX1, dx0, float32v( 1 ) ) - float32v( kUnskew4 );
        float32v dy1 = FS::MaskedSub( maskY1, dy0, float32v( 1 ) ) - float32v( kUnskew4 );
        float32v dz1 = FS::MaskedSub( maskZ1, dz0, float32v( 1 ) ) - float32v( kUnskew4 );
        float32v dw1 = FS::MaskedSub( maskW1, dw0, float32v( 1 ) ) - float32v( kUnskew4 );
        float32v dx2 = FS::MaskedSub( maskX2, dx0, float32v( 1 ) ) - float32v( kUnskew4 * 2 );
        float32v dy2 = FS::MaskedSub( maskY2, dy0, float32v( 1 ) ) - float32v( kUnskew4 * 2 );
        float32v dz2 = FS::MaskedSub( maskZ2, dz0, float32v( 1 ) ) - float32v( kUnskew4 * 2 );
        float32v dw2 = FS::MaskedSub( maskW2, dw0, float32v( 1 ) ) - float32v( kUnskew4 * 2 );
        float32v dx3 = FS::MaskedSub( maskX3, dx0, float32v( 1 ) ) - float32v( kUnskew4 * 3 );
        float32v dy3 = FS::MaskedSub( maskY3, dy0, float32v( 1 ) ) - float32v( kUnskew4 * 3 );
        float32v dz3 = FS::MaskedSub( maskZ3, dz0, float32v( 1 ) ) - float32v( kUnskew4 * 3 );
        float32v dw3 = FS::MaskedSub( maskW3, dw0, float32v( 1 ) ) - float32v( kUnskew4 * 3 );
        float32v dx4 = dx0 - float32v( kUnskew4 * 4 + 1 );
        float32v dy4 = dy0 - float32v( kUnskew4 * 4 + 1 );
        float32v dz4 = dz0 - float32v( kUnskew4 * 4 + 1 );
        float32v dw4 = dw0 - float32v( kUnskew4 * 4 + 1 );

        float32v falloff0 = FS::FNMulAdd( dw0, dw0, FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff1 = FS::FNMulAdd( dw1, dw1, FS::FNMulAdd( dz1, dz1, FS::FNMulAdd( dy1, dy1, FS::FNMulAdd( dx1, dx1, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff2 = FS::FNMulAdd( dw2, dw2, FS::FNMulAdd( dz2, dz2, FS::FNMulAdd( dy2, dy2, FS::FNMulAdd( dx2, dx2, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff3 = FS::FNMulAdd( dw3, dw3, FS::FNMulAdd( dz3, dz3, FS::FNMulAdd( dy3, dy3, FS::FNMulAdd( dx3, dx3, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff4 = falloff0 + FS::FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot5 + 3.0 ) / ( kRoot5 + 5.0 ) ),
            float32v( -4.0 / 5.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );
        falloff3 = FS::Max( falloff3, float32v( 0 ) );
        falloff4 = FS::Max( falloff4, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;
        falloff3 *= falloff3; falloff3 *= falloff3;
        falloff4 *= falloff4; falloff4 *= falloff4;

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v valueW( 0 );

        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0, falloff0, valueX, valueY, valueZ, valueW );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
            FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW1, wPrimedBase, int32v( Primes::W ) ) ), dx1, dy1, dz1, dw1, falloff1, valueX, valueY, valueZ, valueW );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
            FS::MaskedAdd( maskX2, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY2, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ2, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW2, wPrimedBase, int32v( Primes::W ) ) ), dx2, dy2, dz2, dw2, falloff2, valueX, valueY, valueZ, valueW );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
            FS::MaskedAdd( maskX3, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY3, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ3, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW3, wPrimedBase, int32v( Primes::W ) ) ), dx3, dy3, dz3, dw3, falloff3, valueX, valueY, valueZ, valueW );
        ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
            xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ), wPrimedBase + int32v( Primes::W ) ),
            dx4, dy4, dz4, dw4, falloff4, valueX, valueY, valueZ, valueW );

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(33.653125584827855 / 1.4142135623730951) :
            30.88161777516092f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        valueW *= float32v( kBounding * this->mAxisScale[3] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );
        wOut = FS::FMulAdd( valueW, warpAmp, wOut );

        return FS::FMulAdd( valueW, valueW, FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<DomainWarpSuperSimplex, SIMD> final : public virtual DomainWarpSuperSimplex, public DispatchClass<DomainWarp, SIMD>
{
public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_2D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, xOut, yOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_2D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, xOut, yOut );
        }
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        switch( mVectorizationScheme ) 
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_3D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_3D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, xOut, yOut, zOut );
        }        
    }

    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        switch( mVectorizationScheme )
        {
        default:
        case VectorizationScheme::OrthogonalGradientMatrix:
            return Warp_4D<VectorizationScheme::OrthogonalGradientMatrix>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        case VectorizationScheme::GradientOuterProduct:
            return Warp_4D<VectorizationScheme::GradientOuterProduct>( seed, warpAmp, x, y, z, w, xOut, yOut, zOut, wOut );
        }
    }

protected:
    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        constexpr double kRoot3 = 1.7320508075688772935274463415059;
        constexpr double kSkew2 = 1.0 / ( kRoot3 + 1.0 );
        constexpr double kUnskew2 = -1.0 / ( kRoot3 + 3.0 );
        constexpr double kFalloffRadiusSquared = 2.0 / 3.0;

        float32v skewDelta = float32v( kSkew2 ) * ( x + y );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );

        mask32v forwardXY = dxSkewed + dySkewed > float32v( 1.0f );
        float32v boundaryXY = FS::Masked( forwardXY, float32v( -1.0f ) );
        mask32v forwardX = FS::FMulAdd( dxSkewed, float32v( -2.0f ), dySkewed ) < boundaryXY;
        mask32v forwardY = FS::FMulAdd( dySkewed, float32v( -2.0f ), dxSkewed ) < boundaryXY;

        float32v unskewDelta = float32v( kUnskew2 ) * ( dxSkewed + dySkewed );
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;

        float32v falloffBase0;
        float32v valueX( 0 );
        float32v valueY( 0 );

        // Vertex <0, 0>
        {
            int32v hash = HashPrimes( seed, xPrimedBase, yPrimedBase );
            falloffBase0 = FS::FNMulAdd( dxBase, dxBase, FS::FNMulAdd( dyBase, dyBase, float32v( kFalloffRadiusSquared ) ) );
            float32v falloff = falloffBase0; falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dxBase, dyBase, falloff, valueX, valueY );
        }

        // Vertex <1, 1>
        {
            int32v hash = HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) );
            float32v falloff = FS::FMulAdd( unskewDelta,
                float32v( -4.0 * ( kRoot3 + 2.0 ) / ( kRoot3 + 3.0 ) ),
                falloffBase0 - float32v( kFalloffRadiusSquared ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dxBase - float32v( 2 * kUnskew2 + 1 ), dyBase - float32v( 2 * kUnskew2 + 1 ), falloff, valueX, valueY );
        }

        float32v xyDelta = FS::Select( forwardXY, float32v( kUnskew2 + 1 ), float32v( -kUnskew2 ) );
        dxBase -= xyDelta;
        dyBase -= xyDelta;

        // Vertex <1, 0> or <-1, 0> or <1, 2>
        {
            int32v hash = HashPrimes( seed,
                FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardX, xPrimedBase, int32v( Primes::X * 2 ) ), int32v( Primes::X ) ),
                FS::MaskedAdd( forwardXY, yPrimedBase, int32v( Primes::Y ) ) );
            float32v dx = dxBase - FS::Select( forwardX, float32v( 1 + 2 * kUnskew2 ), float32v( -1 ) );
            float32v dy = FS::MaskedSub( forwardX, dyBase, float32v( 2 * kUnskew2 ) );
            float32v falloff = FS::Max( FS::FNMulAdd( dx, dx, FS::FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dx, dy, falloff, valueX, valueY );
        }

        // Vertex <0, 1> or <0, -1> or <2, 1>
        {
            int32v hash = HashPrimes( seed,
                FS::MaskedAdd( forwardXY, xPrimedBase, int32v( Primes::X ) ),
                FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardY, yPrimedBase, int32v( (int32_t)( Primes::Y * 2LL ) ) ), int32v( Primes::Y ) ) );
            float32v dx = FS::MaskedSub( forwardY, dxBase, float32v( 2 * kUnskew2 ) );
            float32v dy = dyBase - FS::Select( forwardY, float32v( 1 + 2 * kUnskew2 ), float32v( -1 ) );
            float32v falloff = FS::Max( FS::FNMulAdd( dx, dx, FS::FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            ApplyVectorContributionSimplex<Scheme>( hash, dx, dy, falloff, valueX, valueY );
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(9.28993664146183 / 2.0) :
            12.814453124999995f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );

        return FS::FMulAdd( valueY, valueY, valueX * valueX );
    }

    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kTwiceUnskew3 = -1.0 / 4.0;

        constexpr double kDistanceSquaredA = 3.0 / 4.0;
        constexpr double kDistanceSquaredB = 1.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        float32v skewDelta = float32v( kSkew3 ) * ( x + y + z );

        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;

        // From unit cell base, find closest vertex
        {
            // Perform a double unskew to get the vector whose dot product with skewed vectors produces the unskewed result.
            float32v twiceUnskewDelta = float32v( kTwiceUnskew3 ) * ( dxSkewed + dySkewed + dzSkewed );
            float32v xNormal = dxSkewed + twiceUnskewDelta;
            float32v yNormal = dySkewed + twiceUnskewDelta;
            float32v zNormal = dzSkewed + twiceUnskewDelta;
            float32v xyzNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal

            // Using those, compare scores to determine which vertex is closest.
            constexpr auto considerVertex = [] ( float32v& maxScore, int32v& moveMaskBits, float32v score, int32v bits ) constexpr
                {
                    moveMaskBits = FS::Select( score > maxScore, bits, moveMaskBits );
                    maxScore = FS::Max( maxScore, score );
                };
            float32v maxScore = float32v( 0.375f );
            int32v moveMaskBits = FS::Masked( xyzNormal > maxScore, int32v( -1 ) );
            maxScore = FS::Max( maxScore, xyzNormal );
            considerVertex( maxScore, moveMaskBits, xNormal, 0b001 );
            considerVertex( maxScore, moveMaskBits, yNormal, 0b010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b100 );
            maxScore += float32v( 0.125f ) - xyzNormal;
            considerVertex( maxScore, moveMaskBits, -zNormal, 0b011 );
            considerVertex( maxScore, moveMaskBits, -yNormal, 0b101 );
            considerVertex( maxScore, moveMaskBits, -xNormal, 0b110 );

            mask32v moveX = ( moveMaskBits & int32v( 0b001 ) ) != int32v( 0 );
            mask32v moveY = ( moveMaskBits & int32v( 0b010 ) ) != int32v( 0 );
            mask32v moveZ = ( moveMaskBits & int32v( 0b100 ) ) != int32v( 0 );

            xSkewedBase = FS::MaskedIncrement( moveX, xSkewedBase );
            ySkewedBase = FS::MaskedIncrement( moveY, ySkewedBase );
            zSkewedBase = FS::MaskedIncrement( moveZ, zSkewedBase );

            dxSkewed = FS::MaskedDecrement( moveX, dxSkewed );
            dySkewed = FS::MaskedDecrement( moveY, dySkewed );
            dzSkewed = FS::MaskedDecrement( moveZ, dzSkewed );
        }

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );

        float32v skewedCoordinateSum = dxSkewed + dySkewed + dzSkewed;
        float32v twiceUnskewDelta = float32v( kTwiceUnskew3 ) * skewedCoordinateSum;
        float32v xNormal = dxSkewed + twiceUnskewDelta;
        float32v yNormal = dySkewed + twiceUnskewDelta;
        float32v zNormal = dzSkewed + twiceUnskewDelta;
        float32v xyzNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal

        float32v unskewDelta = float32v( kReflectUnskew3 ) * skewedCoordinateSum;
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;
        float32v dzBase = dzSkewed + unskewDelta;

        float32v coordinateSum = float32v( 1 + 3 * kReflectUnskew3 ) * skewedCoordinateSum; // dxBase + dyBase + dzBase

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v falloffBaseStemA, falloffBaseStemB;

        // Vertex <0, 0, 0>
        {
            float32v falloffBase = FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dxBase, dyBase, dzBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <1, 1, 1> or <-1, -1, -1>
        {
            mask32v signMask = xyzNormal < float32v( 0 );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset = float32v( 3 * kReflectUnskew3 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimed ), dxBase - offset, dyBase - offset, dzBase - offset,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <1, 1, 0> or <-1, -1, 0>
        {
            mask32v signMask = xyzNormal < zNormal;

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dzBase ) - falloffBaseStemB, float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase ), dxBase, dyBase, dzBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <1, 0, 1> or <-1, 0, -1>
        {
            mask32v signMask = xyzNormal < yNormal;

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dyBase ) - falloffBaseStemB, float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed ), dxBase, dyBase - offset0, dzBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <0, 1, 1> or <0, -1, -1>
        {
            mask32v signMask = xyzNormal < xNormal;

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dxBase ) - falloffBaseStemB, float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed ), dxBase - offset0, dyBase, dzBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <1, 0, 0> or <-1, 0, 0>
        {
            mask32v signMask = xNormal < float32v( 0 );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase ), dxBase + offset0, dyBase - offset0, dzBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <0, 1, 0> or <0, -1, 0>
        {
            mask32v signMask = yNormal < float32v( 0 );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase ), dxBase - offset0, dyBase + offset0, dzBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        // Vertex <0, 0, 1> or <0, 0, -1>
        {
            mask32v signMask = zNormal < float32v( 0 );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            ApplyVectorContributionCommon<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed ), dxBase - offset0, dyBase - offset0, dzBase + offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
        }

        if constexpr( Scheme != VectorizationScheme::OrthogonalGradientMatrix )
        {
            // Match gradient orientation.
            constexpr double kReflect3D = -2.0 / 3.0;
            float32v valueTransformDelta = float32v( kReflect3D ) * ( valueX + valueY + valueZ );
            valueX += valueTransformDelta;
            valueY += valueTransformDelta;
            valueZ += valueTransformDelta;
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(144.736422163332608 / 1.4142135623730951) :
            37.63698669623629f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        return FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) );
    }

    template<VectorizationScheme Scheme>
    float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kTwiceUnskew4 = -1.0 / 5.0;

        constexpr double kDistanceSquaredA = 4.0 / 5.0;
        constexpr double kDistanceSquaredB = 6.0 / 5.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        float32v skewDelta = float32v( kSkew4 ) * ( x + y + z + w );

        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;
        float32v wSkewed = w + skewDelta;
        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v wSkewedBase = FS::Floor( wSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;
        float32v dwSkewed = wSkewed - wSkewedBase;

        // From unit cell base, find closest vertex
        {
            // Perform a double unskew to get the vector whose dot product with skewed vectors produces the unskewed result.
            float32v twiceUnskewDelta = float32v( kTwiceUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
            float32v xNormal = dxSkewed + twiceUnskewDelta;
            float32v yNormal = dySkewed + twiceUnskewDelta;
            float32v zNormal = dzSkewed + twiceUnskewDelta;
            float32v wNormal = dwSkewed + twiceUnskewDelta;
            float32v xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

            // Using those, compare scores to determine which vertex is closest.
            constexpr auto considerVertex = [] ( float32v& maxScore, int32v& moveMaskBits, float32v score, int32v bits ) constexpr
                {
                    moveMaskBits = FS::Select( score > maxScore, bits, moveMaskBits );
                    maxScore = FS::Max( maxScore, score );
                };
            float32v maxScore = float32v( 0.6f ) - xyzwNormal;
            int32v moveMaskBits = FS::Masked( float32v( 0.2f ) > maxScore, int32v( -1 ) );
            maxScore = FS::Max( maxScore, float32v( 0.2f ) );
            considerVertex( maxScore, moveMaskBits, -wNormal, 0b0111 );
            considerVertex( maxScore, moveMaskBits, -zNormal, 0b1011 );
            considerVertex( maxScore, moveMaskBits, -yNormal, 0b1101 );
            considerVertex( maxScore, moveMaskBits, -xNormal, 0b1110 );
            maxScore += xyzwNormal - float32v( 0.2f );
            considerVertex( maxScore, moveMaskBits, xNormal, 0b0001 );
            considerVertex( maxScore, moveMaskBits, yNormal, 0b0010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b0100 );
            considerVertex( maxScore, moveMaskBits, wNormal, 0b1000 );
            maxScore += float32v( 0.2f ) - xNormal;
            considerVertex( maxScore, moveMaskBits, yNormal, 0b0011 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b0101 );
            considerVertex( maxScore, moveMaskBits, wNormal, 0b1001 );
            maxScore += xNormal;
            considerVertex( maxScore, moveMaskBits, yNormal + zNormal, 0b0110 );
            maxScore -= wNormal;
            considerVertex( maxScore, moveMaskBits, yNormal, 0b1010 );
            considerVertex( maxScore, moveMaskBits, zNormal, 0b1100 );

            mask32v moveX = ( moveMaskBits & int32v( 0b0001 ) ) != int32v( 0 );
            mask32v moveY = ( moveMaskBits & int32v( 0b0010 ) ) != int32v( 0 );
            mask32v moveZ = ( moveMaskBits & int32v( 0b0100 ) ) != int32v( 0 );
            mask32v moveW = ( moveMaskBits & int32v( 0b1000 ) ) != int32v( 0 );

            xSkewedBase = FS::MaskedIncrement( moveX, xSkewedBase );
            ySkewedBase = FS::MaskedIncrement( moveY, ySkewedBase );
            zSkewedBase = FS::MaskedIncrement( moveZ, zSkewedBase );
            wSkewedBase = FS::MaskedIncrement( moveW, wSkewedBase );

            dxSkewed = FS::MaskedDecrement( moveX, dxSkewed );
            dySkewed = FS::MaskedDecrement( moveY, dySkewed );
            dzSkewed = FS::MaskedDecrement( moveZ, dzSkewed );
            dwSkewed = FS::MaskedDecrement( moveW, dwSkewed );
        }

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * int32v( Primes::Z );
        int32v wPrimedBase = FS::Convert<int32_t>( wSkewedBase ) * int32v( Primes::W );

        float32v skewedCoordinateSum = dxSkewed + dySkewed + dzSkewed + dwSkewed;
        float32v twiceUnskewDelta = float32v( kTwiceUnskew4 ) * skewedCoordinateSum;
        float32v xNormal = dxSkewed + twiceUnskewDelta;
        float32v yNormal = dySkewed + twiceUnskewDelta;
        float32v zNormal = dzSkewed + twiceUnskewDelta;
        float32v wNormal = dwSkewed + twiceUnskewDelta;
        float32v xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

        float32v unskewDelta = float32v( kUnskew4 ) * skewedCoordinateSum;
        float32v dxBase = dxSkewed + unskewDelta;
        float32v dyBase = dySkewed + unskewDelta;
        float32v dzBase = dzSkewed + unskewDelta;
        float32v dwBase = dwSkewed + unskewDelta;

        float32v coordinateSum = float32v( 1 + 4 * kUnskew4 ) * skewedCoordinateSum; // dxBase + dyBase + dzBase + dwBase

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v valueW( 0 );
        float32v falloffBaseStemA, falloffBaseStemB;

        // Vertex <0, 0, 0, 0>
        {
            float32v falloffBase = FS::FNMulAdd( dwBase, dwBase, FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase, dyBase, dzBase, dwBase,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 1, 1, 1> or <-1, -1, -1, -1>
        {
            mask32v signMask = xyzwNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset = float32v( 4 * kUnskew4 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimed, wPrimed ), dxBase - offset, dyBase - offset, dzBase - offset, dwBase - offset,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 1, 1, 0> or <-1, -1, -1, 0>
        {
            mask32v signMask = xyzwNormal < wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dwBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimed, wPrimedBase ), dxBase - offset1, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 1, 0, 1> or <-1, -1, 0, -1>
        {
            mask32v signMask = xyzwNormal < zNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dzBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase, wPrimed ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 0, 1, 1> or <-1, 0, -1, -1>
        {
            mask32v signMask = xyzwNormal < yNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dyBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed, wPrimed ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 1, 1, 1> or <0, -1, -1, -1>
        {
            mask32v signMask = xyzwNormal < xNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dxBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed, wPrimed ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 0, 0, 0> or <-1, 0, 0, 0>
        {
            mask32v signMask = xNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 1, 0, 0> or <-1, -1, 0, 0>
        {
            mask32v signMask = xNormal < -yNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dyBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase, wPrimedBase ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 0, 1, 0> or <-1, 0, -1, 0>
        {
            mask32v signMask = xNormal < -zNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dzBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed, wPrimedBase ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <1, 0, 0, 1> or <-1, 0, 0, -1>
        {
            mask32v signMask = xNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dwBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase, wPrimed ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 1, 0, 0> or <0, -1, 0, 0>
        {
            mask32v signMask = yNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase, wPrimedBase ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 1, 1, 0> or <0, -1, -1, 0>
        {
            mask32v signMask = yNormal < -zNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dzBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed, wPrimedBase ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 1, 0, 1> or <0, -1, 0, -1>
        {
            mask32v signMask = yNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dwBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase, wPrimed ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 0, 1, 0> or <0, 0, -1, 0>
        {
            mask32v signMask = zNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed, wPrimedBase ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 0, 1, 1> or <0, 0, -1, -1>
        {
            mask32v signMask = zNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dzBase + dwBase ) ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed, wPrimed ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        // Vertex <0, 0, 0, 1> or <0, 0, 0, -1>
        {
            mask32v signMask = wNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dwBase ), float32v( 0.0f ) );

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimed ), dxBase - offset0, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
        }

        constexpr float kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            (float)(115.21625311930542 / 1.4142135623730951) :
            48.80058117543753f );

        valueX *= float32v( kBounding * this->mAxisScale[0] );
        valueY *= float32v( kBounding * this->mAxisScale[1] );
        valueZ *= float32v( kBounding * this->mAxisScale[2] );
        valueW *= float32v( kBounding * this->mAxisScale[3] );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );
        wOut = FS::FMulAdd( valueW, warpAmp, wOut );

        return FS::FMulAdd( valueW, valueW, FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};
