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
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX ) || kLatticeBlockTbl<SIMD> ) // 8 entry table lookups: vpermps / AArch64 TBL
        {
            float32v finalSign = FS::Cast<float>( ( ( indexGradient >> 28 ) ^ indexOuterVector ) << 31 );
            indexGradient >>= 29;
            indexOuterVector = ( indexOuterVector >> 1 ) | ( indexOuterVector >> 29 );
            if constexpr( !( SIMD & FastSIMD::FeatureFlag::AVX ) )
            {
                // vpermps only reads the low 3 bits (the shifts above are arithmetic), AArch64 TBL reads 0 past the table
                indexGradient &= int32v( 7 );
                indexOuterVector &= int32v( 7 );
            }

            float32v gX = LatticeBlockPermute<float32v>( FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), indexGradient );
            float32v gY = LatticeBlockPermute<float32v>( FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), indexGradient );

            multiplier *= FS::FMulAdd( fY, gY, fX * gX ) ^ finalSign;

            valueX = FS::FMulAdd( multiplier, LatticeBlockPermute<float32v>( FS::Constant<float>( kRoot3f, 1, kRoot3f, -1, 2, 0, 0, 0 ), indexOuterVector ), valueX );
            valueY = FS::FMulAdd( multiplier, LatticeBlockPermute<float32v>( FS::Constant<float>( 1, kRoot3f, -1, kRoot3f, 0, 2, 0, 0 ), indexOuterVector ), valueY );
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
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) || kLatticeBlockTbl<SIMD> )
        {
            // Same table as AVX512 split into two 8 entry halves, entries [16,20) are entries [0,4) negated.
            // The hash has a clear sign bit so the index is [0,20) << 27, which makes the high half ( [8,16) << 27 ) a signed
            // compare. Testing bit 30 with index << 1 instead gets folded into an extra vpmulld by clang
            auto lookup = []( int32v index, const auto& tableLow, const auto& tableHigh )
            {
                int32v permuteIndex = FS::BitShiftRightZeroExtend( index, 27 );
                if constexpr( !( SIMD & FastSIMD::FeatureFlag::AVX2 ) )
                {
                    permuteIndex &= int32v( 7 ); // vpermps only reads the low 3 bits, AArch64 TBL reads 0 past the table
                }
                float32v low = LatticeBlockPermute<float32v>( tableLow, permuteIndex );
                float32v high = LatticeBlockPermute<float32v>( tableHigh, permuteIndex );

                return FS::Select( index > int32v( 0x3FFFFFFF ), high, low );
            };

            const auto tableLowX = FS::Constant<float>( kSkew4f + 1, kSkew4f, kSkew4f, kSkew4f, -1, 1, 0, 0 );
            const auto tableLowY = FS::Constant<float>( kSkew4f, kSkew4f + 1, kSkew4f, kSkew4f, 1, -1, 0, 0 );
            const auto tableLowZ = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f + 1, kSkew4f, 0, 0, -1, 1 );
            const auto tableLowW = FS::Constant<float>( kSkew4f, kSkew4f, kSkew4f, kSkew4f + 1, 0, 0, 1, -1 );
            const auto tableHighX = FS::Constant<float>( -1, 0, 1, 0, -1, 0, 0, 1 );
            const auto tableHighY = FS::Constant<float>( 0, -1, 0, 1, 0, -1, 1, 0 );
            const auto tableHighZ = FS::Constant<float>( 1, 0, -1, 0, 0, 1, -1, 0 );
            const auto tableHighW = FS::Constant<float>( 0, 1, 0, -1, 1, 0, 0, -1 );

            float32v gX = lookup( indexGradient, tableLowX, tableHighX );
            float32v gY = lookup( indexGradient, tableLowY, tableHighY );
            float32v gZ = lookup( indexGradient, tableLowZ, tableHighZ );
            float32v gW = lookup( indexGradient, tableLowW, tableHighW );

            // The negated entries flip every component, so apply their sign to the dot product and the multiplier instead
            multiplier *= FS::FMulAdd( gW, fW, FS::FMulAdd( gZ, fZ, FS::FMulAdd( gY, fY, gX * fX ) ) ) ^ FS::Cast<float>( indexGradient & int32v( 1 << 31 ) );
            multiplier ^= FS::Cast<float>( indexOuterVector & int32v( 1 << 31 ) );

            valueX = FS::FMulAdd( multiplier, lookup( indexOuterVector, tableLowX, tableHighX ), valueX );
            valueY = FS::FMulAdd( multiplier, lookup( indexOuterVector, tableLowY, tableHighY ), valueY );
            valueZ = FS::FMulAdd( multiplier, lookup( indexOuterVector, tableLowZ, tableHighZ ), valueZ );
            valueW = FS::FMulAdd( multiplier, lookup( indexOuterVector, tableLowW, tableHighW ), valueW );
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
    FN_ALIGN_STACK static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductSimplex( int32v hash31, float32v fX, float32v fY, float32v multiplier, float32v& valueX, float32v& valueY )
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
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) || kLatticeBlockTbl<SIMD> ) // 8 entry table lookups: vpermps / AArch64 TBL
        {
            float32v signX = FS::Cast<float>( ( index >> 28 ) << 31 );
            index = FS::BitShiftRightZeroExtend( index, 29 );

            float32v gX = LatticeBlockPermute<float32v>( FS::Constant<float>( kSkew2f, kSkew2f, kSkew2f + 1, kSkew2f + 1, 1, 1, 0, 0 ), index ) ^ signX;
            float32v gY = LatticeBlockPermute<float32v>( FS::Constant<float>( kSkew2f + 1, -kSkew2f - 1, kSkew2f, -kSkew2f, 1, -1, 0, 0 ), index );

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

    template<FastSIMD::FeatureSet SIMD, typename I, typename F>
    FS_FORCEINLINE static void ApplyOrthogonalGradientMatrixVectorProductCommonImpl( I hash31, F fX, F fY, F fZ, F multiplier, F& valueX, F& valueY, F& valueZ )
    {
        constexpr float kComponentA = 2.224744871391589f;
        constexpr float kComponentB = -0.224744871391589f;
        constexpr float kComponentC = -1.0f;
        constexpr float kComponentsDE = 1.0f;
        constexpr float kComponentF = 2.0f;

        I hashShifted = FS::BitShiftRightZeroExtend( hash31, 1 );
        I indexFacetBasisWithPermute2 = hashShifted * I( 0xAAAAAAAB ); // [0,3) in the highest two bits, [0,8) in the lowest three bits // ( -4LL << 30 ) / 3
        I indexPermutation2HighBit = ( indexFacetBasisWithPermute2 << 29 ); // & I( 1 << 31 ); // [0,1) in the most significant bit
        I indexPermutation3 = FS::BitShiftRightZeroExtend( hashShifted * I( 3 ), 30 ); // [0,3)
        F finalSign = FS::Cast<float>( hash31 << 31 );

        F valueAB, valueBA, valueC;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexFacetBasisWithPermute2 = FS::NativeExec<I>( []( auto a ){ return _mm512_rol_epi32( a, 2 ); }, indexFacetBasisWithPermute2 );

            const auto tableA_gX = FS::Constant<float>( kComponentA, kComponentA, kComponentC, kComponentC, -kComponentA, -kComponentA, kComponentC, kComponentC, kComponentA, kComponentA, kComponentC, kComponentC, -kComponentA, -kComponentA, kComponentC, kComponentC );
            const auto tableA_gY = FS::Constant<float>( kComponentC, kComponentB, kComponentA, kComponentA, kComponentC, kComponentB, -kComponentA, -kComponentA, kComponentC, -kComponentB, kComponentA, kComponentA, kComponentC, -kComponentB, -kComponentA, -kComponentA );
            const auto tableA_gZ = FS::Constant<float>( kComponentB, kComponentC, kComponentB, kComponentB, kComponentB, kComponentC, kComponentB, kComponentB, -kComponentB, kComponentC, -kComponentB, -kComponentB, -kComponentB, kComponentC, -kComponentB, -kComponentB );

            const auto tableB_gX = FS::Constant<float>( kComponentB, kComponentB, kComponentC, kComponentC, -kComponentB, -kComponentB, kComponentC, kComponentC, kComponentB, kComponentB, kComponentC, kComponentC, -kComponentB, -kComponentB, kComponentC, kComponentC );
            const auto tableB_gY = FS::Constant<float>( kComponentC, kComponentA, kComponentB, kComponentB, kComponentC, kComponentA, -kComponentB, -kComponentB, kComponentC, -kComponentA, kComponentB, kComponentB, kComponentC, -kComponentA, -kComponentB, -kComponentB );
            const auto tableB_gZ = FS::Constant<float>( kComponentA, kComponentC, kComponentA, kComponentA, kComponentA, kComponentC, kComponentA, kComponentA, -kComponentA, kComponentC, -kComponentA, -kComponentA, -kComponentA, kComponentC, -kComponentA, -kComponentA );

            const auto tableC_gX = FS::Constant<float>( kComponentsDE, kComponentsDE, kComponentF, kComponentF, kComponentC, kComponentC, kComponentF, kComponentF, kComponentsDE, kComponentsDE, kComponentF, kComponentF, kComponentC, kComponentC, kComponentF, kComponentF );
            const auto tableC_gY = FS::Constant<float>( kComponentF, kComponentsDE, kComponentsDE, kComponentsDE, kComponentF, kComponentsDE, kComponentC, kComponentC, kComponentF, kComponentC, kComponentsDE, kComponentsDE, kComponentF, kComponentC, kComponentC, kComponentC );
            const auto tableC_gZ = FS::Constant<float>( kComponentsDE, kComponentF, kComponentsDE, kComponentsDE, kComponentsDE, kComponentF, kComponentsDE, kComponentsDE, kComponentC, kComponentF, kComponentC, kComponentC, kComponentC, kComponentF, kComponentC, kComponentC );

            F valueAB_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gX, indexFacetBasisWithPermute2, tableB_gX );
            F valueAB_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gY, indexFacetBasisWithPermute2, tableB_gY );
            F valueAB_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableA_gZ, indexFacetBasisWithPermute2, tableB_gZ );
            valueAB = FS::FMulAdd( valueAB_gZ, fZ, FS::FMulAdd( fY, valueAB_gY, fX * valueAB_gX ) );

            F valueBA_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gX, indexFacetBasisWithPermute2, tableA_gX );
            F valueBA_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gY, indexFacetBasisWithPermute2, tableA_gY );
            F valueBA_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), tableB_gZ, indexFacetBasisWithPermute2, tableA_gZ );
            valueBA = FS::FMulAdd( valueBA_gZ, fZ, FS::FMulAdd( fY, valueBA_gY, fX * valueBA_gX ) );

            F valueC_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gX );
            F valueC_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gY );
            F valueC_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutexvar_ps ), indexFacetBasisWithPermute2, tableC_gZ );
            valueC = FS::FMulAdd( valueC_gZ, fZ, FS::FMulAdd( fY, valueC_gY, fX * valueC_gX ) );
        }
        else
        {
            F sign0 = FS::Cast<float>( indexFacetBasisWithPermute2 << 31 );
            F sign1 = FS::Cast<float>( ( indexFacetBasisWithPermute2 << 30 ) & I( 1 << 31 ) );

            auto notYZ = indexFacetBasisWithPermute2;
            auto notXY = indexFacetBasisWithPermute2 << 1;

            // The facet permutes which axis gets which gradient component: ( A, C, B ), ( A, B, C ) with notXY, ( C, A, B ) with notYZ
            // ( never both, hash31 has a clear sign bit so the top two bits are [0,3) ). Apply that permutation to f once instead of
            // selecting every component of the three gradients
            F fA = FS::SelectHighBit( notYZ, fY, fX );
            F fB = FS::SelectHighBit( notXY, fY, fZ );
            F fC = FS::SelectHighBit( notYZ, fX, FS::SelectHighBit( notXY, fZ, fY ) );
            F productC = fC * F( kComponentC );

            F valueA = FS::FMulAdd( F( kComponentA ) ^ sign0, fA, FS::FMulAdd( F( kComponentB ) ^ sign1, fB, productC ) );
            F valueB = FS::FMulAdd( F( kComponentB ) ^ sign0, fA, FS::FMulAdd( F( kComponentA ) ^ sign1, fB, productC ) );
            valueC = FS::FMulAdd( F( kComponentsDE ) ^ sign0, fA, FS::FMulAdd( F( kComponentsDE ) ^ sign1, fB, fC * F( kComponentF ) ) );

            valueAB = FS::SelectHighBit( indexPermutation2HighBit, valueB, valueA );
            valueBA = FS::SelectHighBit( indexPermutation2HighBit, valueA, valueB );
        }

        multiplier ^= finalSign;
        valueX = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == I( 0 ), valueC, valueAB ), valueX );
        valueY = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == I( 1 ), valueC, FS::Select( indexPermutation3 == I( 2 ), valueBA, valueAB ) ), valueY );
        valueZ = FS::FMulAdd( multiplier, FS::Select( indexPermutation3 == I( 2 ), valueC, valueBA ), valueZ );
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductCommon( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ )
    {
        ApplyOrthogonalGradientMatrixVectorProductCommonImpl<SIMD>( hash31, fX, fY, fZ, multiplier, valueX, valueY, valueZ );
    }

    template<FastSIMD::FeatureSet SIMD, typename I, typename F>
    FS_FORCEINLINE static void ApplyOrthogonalGradientMatrixVectorProductSimplexImpl( I hash31, F fX, F fY, F fZ, F fW, F multiplier, F& valueX, F& valueY, F& valueZ, F& valueW )
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

        I hashShifted = FS::BitShiftRightZeroExtend( hash31, 2 );
        I indexBasis = hashShifted * I( 20 >> 2 ); // [0,20) << 27
        I indexPermutation3 = ( hashShifted * I( 0xD5555556 ) ) >> 29; // [0,3] // ( -4LL << 29 ) / 3
        I indexPermutation8 = indexBasis >> 24; // & I( 0x07 );
        F finalSign = FS::Cast<float>( hash31 << 31 );

        F valueA, valueB, valueC, valueD;
        F valueA_gX, valueB_gX, valueC_gX, valueD_gX;
        F valueA_gY, valueB_gY, valueC_gY, valueD_gY;
        F valueA_gZ, valueB_gZ, valueC_gZ, valueD_gZ;
        F valueA_gW, valueB_gW, valueC_gW, valueD_gW;

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            indexBasis >>= 27;

            valueA_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sDiagABC, sDiagABC, sDiagABC, pIdxPosAB, pIdxNegAB, pDiagAB, pDiagAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pDiagAB, pDiagAB, pIdxNegAB ), indexBasis,
                FS::Constant<float>( eIdxABC, eDiagABC, eDiagABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sFillABC, sFillABC, sFillABC, pIdxPosAB, pIdxNegAB, pFillAB, pFillAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pFillAB, pFillAB, pIdxNegAB ), indexBasis,
                FS::Constant<float>( eIdxABC, eFillABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxABC, sFillABC, sFillABC, sFillABC, pIdxPosC, pIdxNegC, pFillC, pFillC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pFillC, pFillC, pIdxNegC ), indexBasis,
                FS::Constant<float>( eIdxABC, eFillABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sIdxD, sFillD, sFillD, sFillD, pIdxPosD, pIdxNegD, pFillD, pFillD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pFillD, pFillD, pIdxNegD ), indexBasis,
                FS::Constant<float>( eIdxD, eFillD, eFillD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = valueA_gX * fX;
            valueB = valueB_gX * fX;
            valueC = valueC_gX * fX;
            valueD = valueD_gX * fX;

            valueA_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sIdxABC, sFillABC, sFillABC, pIdxNegAB, pIdxPosAB, pFillAB, pFillAB, pDiagAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pIdxNegAB, pDiagAB ), indexBasis,
                FS::Constant<float>( eDiagABC, eIdxABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sIdxABC, sDiagABC, sDiagABC, pIdxNegAB, pIdxPosAB, pDiagAB, pDiagAB, pFillAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pIdxNegAB, pFillAB ), indexBasis,
                FS::Constant<float>( eFillABC, eIdxABC, eDiagABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sIdxABC, sFillABC, sFillABC, pIdxNegC, pIdxPosC, pFillC, pFillC, pFillC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pIdxNegC, pFillC ), indexBasis,
                FS::Constant<float>( eFillABC, eIdxABC, eFillABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sIdxD, sFillD, sFillD, pIdxNegD, pIdxPosD, pFillD, pFillD, pFillD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pIdxNegD, pFillD ), indexBasis,
                FS::Constant<float>( eFillD, eIdxD, eFillD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gY, fY, valueA );
            valueB = FS::FMulAdd( valueB_gY, fY, valueB );
            valueC = FS::FMulAdd( valueC_gY, fY, valueC );
            valueD = FS::FMulAdd( valueD_gY, fY, valueD );

            valueA_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sIdxABC, sFillABC, pDiagAB, pDiagAB, pIdxPosAB, pIdxNegAB, pIdxNegAB, pFillAB, pIdxPosAB, pFillAB, pFillAB, pIdxNegAB, pIdxPosAB, pFillAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eIdxABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sDiagABC, sIdxABC, sFillABC, pFillAB, pFillAB, pIdxPosAB, pIdxNegAB, pIdxNegAB, pDiagAB, pIdxPosAB, pDiagAB, pDiagAB, pIdxNegAB, pIdxPosAB, pDiagAB ), indexBasis,
                FS::Constant<float>( eDiagABC, eDiagABC, eIdxABC, eFillABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sIdxABC, sDiagABC, pFillC, pFillC, pIdxPosC, pIdxNegC, pIdxNegC, pFillC, pIdxPosC, pFillC, pFillC, pIdxNegC, pIdxPosC, pFillC ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eIdxABC, eDiagABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sFillD, sIdxD, sFillD, pFillD, pFillD, pIdxPosD, pIdxNegD, pIdxNegD, pFillD, pIdxPosD, pFillD, pFillD, pIdxNegD, pIdxPosD, pFillD ), indexBasis,
                FS::Constant<float>( eFillD, eFillD, eIdxD, eFillD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gZ, fZ, valueA );
            valueB = FS::FMulAdd( valueB_gZ, fZ, valueB );
            valueC = FS::FMulAdd( valueC_gZ, fZ, valueC );
            valueD = FS::FMulAdd( valueD_gZ, fZ, valueD );

            valueA_gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sFillABC, sIdxABC, pFillAB, pFillAB, pIdxNegAB, pIdxPosAB, pFillAB, pIdxNegAB, pFillAB, pIdxPosAB, pIdxNegAB, pFillAB, pFillAB, pIdxPosAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eFillABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueB_gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillABC, sFillABC, sFillABC, sIdxABC, pDiagAB, pDiagAB, pIdxNegAB, pIdxPosAB, pDiagAB, pIdxNegAB, pDiagAB, pIdxPosAB, pIdxNegAB, pDiagAB, pDiagAB, pIdxPosAB ), indexBasis,
                FS::Constant<float>( eFillABC, eFillABC, eFillABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueC_gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sDiagABC, sDiagABC, sDiagABC, sIdxABC, pFillC, pFillC, pIdxNegC, pIdxPosC, pFillC, pIdxNegC, pFillC, pIdxPosC, pIdxNegC, pFillC, pFillC, pIdxPosC ), indexBasis,
                FS::Constant<float>( eDiagABC, eDiagABC, eDiagABC, eIdxABC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );
            valueD_gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ),
                FS::Constant<float>( sFillD, sFillD, sFillD, sIdxD, pFillD, pFillD, pIdxNegD, pIdxPosD, pFillD, pIdxNegD, pFillD, pIdxPosD, pIdxNegD, pFillD, pFillD, pIdxPosD ), indexBasis,
                FS::Constant<float>( eFillD, eFillD, eFillD, eIdxD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )
            );

            valueA = FS::FMulAdd( valueA_gW, fW, valueA );
            valueB = FS::FMulAdd( valueB_gW, fW, valueB );
            valueC = FS::FMulAdd( valueC_gW, fW, valueC );
            valueD = FS::FMulAdd( valueD_gW, fW, valueD );
        }
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::SSE41 ) || kLatticeBlockTbl<SIMD> )
        {
            // Every basis gives the axes four roles: the ( positive ) indexed axis, the first and second unindexed axes and
            // either the negative indexed axis ( pairwise ) or the third unindexed axis ( single/extra ). With f gathered in
            // role order the gradients only depend on that case, A and B add the diagonal delta on the first/second unindexed axis.
            // The role axes are a 16 entry table ( [16,20) use the same axes as [0,4) ) packed in the top byte, two bits per role
            I roleAxes;
            if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
            {
                I permuteIndex = FS::BitShiftRightZeroExtend( indexBasis, 27 );
                roleAxes = FS::Select( indexBasis > I( 0x3FFFFFFF ), // [8,16) << 27
                    FS::NativeExec<I>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_epi32 ), FS::Constant<std::int32_t>( 0x1E000000, 0x4B000000, 0x9C000000, 0xC9000000, 0x1B000000, 0x4E000000, 0x8D000000, 0xD8000000 ), permuteIndex ),
                    FS::NativeExec<I>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_epi32 ), FS::Constant<std::int32_t>( 0x1B000000, 0x4B000000, 0x87000000, 0xC6000000, 0x2D000000, 0x6C000000, 0x87000000, 0xC6000000 ), permuteIndex ) );
            }
            else // Byte shuffle the table into the top byte, the other bytes of the shuffle index have the high bit set and become 0
            {
                I shuffleIndex = ( FS::BitShiftRightZeroExtend( indexBasis, 3 ) & I( 0x0F000000 ) ) | I( 0x00808080 );
                auto table = FS::Constant<std::int32_t>( 0xC6874B1B, 0xC6876C2D, 0xC99C4B1E, 0xD88D4E1B );

                if constexpr( SIMD & FastSIMD::FeatureFlag::SSE41 )
                {
                    roleAxes = FS::NativeExec<I>( FS_BIND_INTRINSIC( _mm_shuffle_epi8 ), table, shuffleIndex );
                }
#if FN_AARCH64_TBL
                else // AArch64 TBL: indices >= 16 also become 0
                {
                    roleAxes = FS::NativeExec<I>( []( int32x4_t t, int32x4_t idx ) { return vreinterpretq_s32_u8( vqtbl1q_u8( vreinterpretq_u8_s32( t ), vreinterpretq_u8_s32( idx ) ) ); }, table, shuffleIndex );
                }
#endif
            }

            auto gatherRole = [&]( I axisBits )
            {
                I lowBit = axisBits << 1;
                return FS::SelectHighBit( axisBits, FS::SelectHighBit( lowBit, fW, fZ ), FS::SelectHighBit( lowBit, fY, fX ) );
            };

            F f0 = gatherRole( roleAxes );
            F f1 = gatherRole( roleAxes << 2 );
            F f2 = gatherRole( roleAxes << 4 );
            F f3 = gatherRole( roleAxes << 6 );
            F f12 = f1 + f2;

            // Single or extra ( [0,4) << 27, or [16,20) << 27 which is negative )
            auto singleLayout = indexBasis < I( 4 << 27 );

            auto caseConstant = [&]( float pairwise, float single, float extra )
            {
                if constexpr( SIMD & FastSIMD::FeatureFlag::AVX2 )
                {
                    // Low 3 bits: 0 pairwise, 7 single, 6 extra
                    I caseIndex = FS::Cast<std::int32_t>( singleLayout ) + FS::Cast<std::int32_t>( indexBasis < I( 0 ) );
                    return FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( pairwise, 0, 0, 0, 0, 0, extra, single ), caseIndex );
                }
                else
                {
                    return FS::Select( singleLayout, FS::SelectHighBit( indexBasis, F( extra ), F( single ) ), F( pairwise ) );
                }
            };

            F valueAB = FS::FMulAdd( caseConstant( pIdxNegAB, sFillABC, eFillABC ), f3, FS::FMulAdd( caseConstant( pFillAB, sFillABC, eFillABC ), f12, caseConstant( pIdxPosAB, sIdxABC, eIdxABC ) * f0 ) );
            valueA = FS::FMulAdd( F( kDeltaUnindexedFillerToDiagonal ), f1, valueAB );
            valueB = FS::FMulAdd( F( kDeltaUnindexedFillerToDiagonal ), f2, valueAB );
            valueC = FS::FMulAdd( caseConstant( pIdxNegC, sDiagABC, eDiagABC ), f3, FS::FMulAdd( caseConstant( pFillC, sFillABC, eFillABC ), f12, caseConstant( pIdxPosC, sIdxABC, eIdxABC ) * f0 ) );
            valueD = FS::FMulAdd( caseConstant( pIdxNegD, sFillD, eFillD ), f3, FS::FMulAdd( caseConstant( pFillD, sFillD, eFillD ), f12, caseConstant( pIdxPosD, sIdxD, eIdxD ) * f0 ) );
        }
        else
        {
            I indexPositive = indexBasis & I( 0x03 << 27 );
            I indexNegative = ( indexBasis >> 2 ) & I( 0x03 << 27 );
            indexNegative ^= indexPositive;

            auto extraCase = ( indexBasis < I( 0 ) ); // [16,20) << 27 sets the sign bit
            auto singleCase = ( indexPositive == indexNegative );
            auto singleNonExtraCase = FS::BitwiseAndNot( singleCase, extraCase );
            indexPositive |= FS::Cast<int32_t>( singleNonExtraCase ); // Force indexPositive checks to fail

            F singleOffsetAB = FS::MaskedAdd( extraCase, F( kDeltaPairwiseToSingleAB ), F( kDeltaSingleToExtra ) );
            F componentIndexedNegativeAB = FS::MaskedAdd( singleCase, F( kComponentPairwiseIndexedNegativeAB ), singleOffsetAB );
            F componentUnindexedFillerAB = FS::MaskedAdd( singleCase, F( kComponentPairwiseUnindexedFillerAB ), singleOffsetAB );

            F componentIndexedNegativeC = FS::InvMaskedAdd( singleCase, componentIndexedNegativeAB, F( kDeltaPairwiseABToC ) );
            F componentUnindexedFillerC = FS::InvMaskedAdd( singleCase, componentUnindexedFillerAB, F( kDeltaPairwiseABToC ) );

            F singleOffsetD = FS::MaskedAdd( extraCase, F( kDeltaPairwiseToSingleD ), F( kDeltaSingleToExtra ) );
            F componentIndexedPositiveD = FS::MaskedAdd( singleCase, F( kComponentPairwiseIndexedPositiveD ), singleOffsetD );
            F componentUnindexedD = FS::MaskedAdd( singleCase, F( kComponentPairwiseUnindexedD ), singleOffsetD );

            I indexedCounter( -1 );

            {
                auto indexedPositive = ( indexPositive == I( 0 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == I( 0 << 27 ) );

                F indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                F indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                F indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                F unindexedComponentA = componentUnindexedFillerAB + F( kDeltaUnindexedFillerToDiagonal );
                F unindexedComponentB = componentUnindexedFillerAB;
                F unindexedComponentC = componentUnindexedFillerC;

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
                auto indexedPositive = ( indexPositive == I( 1 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == I( 1 << 27 ) );

                F indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                F indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                F indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                F unindexedComponentA = FS::MaskedAdd( indexedCounter == I( -2 ), componentUnindexedFillerAB, F( kDeltaUnindexedFillerToDiagonal ) );
                F unindexedComponentB = FS::MaskedAdd( indexedCounter == I( -1 ), componentUnindexedFillerAB, F( kDeltaUnindexedFillerToDiagonal ) );
                F unindexedComponentC = componentUnindexedFillerC;

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
                auto indexedPositive = ( indexPositive == I( 2 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == I( 2 << 27 ) );

                F indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                F indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                F indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                F unindexedComponentA = FS::MaskedAdd( indexedCounter == I( -3 ), componentUnindexedFillerAB, F( kDeltaUnindexedFillerToDiagonal ) );
                F unindexedComponentB = FS::MaskedAdd( indexedCounter == I( -2 ), componentUnindexedFillerAB, F( kDeltaUnindexedFillerToDiagonal ) );
                F unindexedComponentC = FS::MaskedAdd( indexedCounter == I( -1 ), componentUnindexedFillerC,  F( kDeltaUnindexedFillerToDiagonal ) );

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
                auto indexedPositive = ( indexPositive == I( 3 << 27 ) );
                auto indexed = indexedPositive | ( indexNegative == I( 3 << 27 ) );

                F indexedComponentAB = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeAB );
                F indexedComponentC = FS::MaskedIncrement( indexedPositive, componentIndexedNegativeC );
                F indexedComponentD = FS::MaskedDecrement( ~indexedPositive, componentIndexedPositiveD );

                F unindexedComponentA = componentUnindexedFillerAB;
                F unindexedComponentB = FS::MaskedAdd( indexedCounter == I( -3 ), componentUnindexedFillerAB, F( kDeltaUnindexedFillerToDiagonal ) );
                F unindexedComponentC = FS::MaskedAdd( indexedCounter == I( -2 ), componentUnindexedFillerC,  F( kDeltaUnindexedFillerToDiagonal ) );

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

        I valueIndexX = ( indexPermutation8 >> 1 ); // & I( 0x3 );
        I valueIndexY = ( FS::Increment( valueIndexX ) + indexPermutation3 ); // & I( 0x3 );
        I valueIndexZ = indexPermutation8 & I( 0x1 );
        valueIndexZ = ( FS::Increment( valueIndexX ) + FS::MaskedIncrement( valueIndexZ >= indexPermutation3, valueIndexZ ) ); // & I( 0x3 );
        I valueIndexSumXYZ = valueIndexX + valueIndexY + valueIndexZ;

        multiplier ^= finalSign;
        valueX = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexX << 31, FS::SelectHighBit( valueIndexX << 30, valueD, valueB ), FS::SelectHighBit( valueIndexX << 30, valueC, valueA ) ), valueX );
        valueY = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexY << 31, FS::SelectHighBit( valueIndexY << 30, valueD, valueB ), FS::SelectHighBit( valueIndexY << 30, valueC, valueA ) ), valueY );
        valueZ = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexZ << 31, FS::SelectHighBit( valueIndexZ << 30, valueD, valueB ), FS::SelectHighBit( valueIndexZ << 30, valueC, valueA ) ), valueZ );
        valueW = FS::FMulAdd( multiplier, FS::SelectHighBit( valueIndexSumXYZ << 31, FS::SelectHighBit( valueIndexSumXYZ << 30, valueD, valueB ), FS::SelectHighBit( valueIndexSumXYZ << 30, valueA, valueC ) ), valueW );
    }

    template<FastSIMD::FeatureSet SIMD = FastSIMD::FeatureSetDefault()>
    FN_ALIGN_STACK static void FS_VECTORCALL ApplyOrthogonalGradientMatrixVectorProductSimplex( int32v hash31, float32v fX, float32v fY, float32v fZ, float32v fW, float32v multiplier, float32v& valueX, float32v& valueY, float32v& valueZ, float32v& valueW )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            ApplyOrthogonalGradientMatrixVectorProductSimplexImpl<SIMD>( hash31, fX, fY, fZ, fW, multiplier, valueX, valueY, valueZ, valueW );
        }
        else // Process each native register half separately, skipping halves with no contribution
        {
            if( FS::AnyMask( multiplier.v0 != float32v::DoubledType( 0 ) ) )
            {
                ApplyOrthogonalGradientMatrixVectorProductSimplexImpl<SIMD>( hash31.v0, fX.v0, fY.v0, fZ.v0, fW.v0, multiplier.v0, valueX.v0, valueY.v0, valueZ.v0, valueW.v0 );
            }
            if( FS::AnyMask( multiplier.v1 != float32v::DoubledType( 0 ) ) )
            {
                ApplyOrthogonalGradientMatrixVectorProductSimplexImpl<SIMD>( hash31.v1, fX.v1, fY.v1, fZ.v1, fW.v1, multiplier.v1, valueX.v1, valueY.v1, valueZ.v1, valueW.v1 );
            }
        }
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
    FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> );

public:
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
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
    FS_FORCEINLINE float32v Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
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
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
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
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
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

        float32v valueX( 0 );
        float32v valueY( 0 );
        float32v valueZ( 0 );
        float32v valueW( 0 );

        // Each vertex is computed right before its contribution so fewer values stay live across the contribution calls
        float32v falloff0 = FS::FNMulAdd( dw0, dw0, FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) ) );
        float32v falloff4 = falloff0 + FS::FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot5 + 3.0 ) / ( kRoot5 + 5.0 ) ),
            float32v( -4.0 / 5.0 ) );
        {
            float32v falloff = FS::Max( falloff0, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0, falloff, valueX, valueY, valueZ, valueW );
        }

        // Vertex i (1 to 3) steps along the axes ranked above 3 - i
        auto applyRankedVertex = [&]( int rankThreshold, float unskewOffset )
        {
            mask32v maskX = rankX > int32v( rankThreshold );
            mask32v maskY = rankY > int32v( rankThreshold );
            mask32v maskZ = rankZ > int32v( rankThreshold );
            mask32v maskW = rankW > int32v( rankThreshold );

            float32v dx = FS::MaskedSub( maskX, dx0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dy = FS::MaskedSub( maskY, dy0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dz = FS::MaskedSub( maskZ, dz0, float32v( 1 ) ) - float32v( unskewOffset );
            float32v dw = FS::MaskedSub( maskW, dw0, float32v( 1 ) ) - float32v( unskewOffset );

            float32v falloff = FS::FNMulAdd( dw, dw, FS::FNMulAdd( dz, dz, FS::FNMulAdd( dy, dy, FS::FNMulAdd( dx, dx, float32v( kFalloffRadiusSquared ) ) ) ) );
            falloff = FS::Max( falloff, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
                FS::MaskedAdd( maskX, xPrimedBase, int32v( Primes::X ) ),
                FS::MaskedAdd( maskY, yPrimedBase, int32v( Primes::Y ) ),
                FS::MaskedAdd( maskZ, zPrimedBase, int32v( Primes::Z ) ),
                FS::MaskedAdd( maskW, wPrimedBase, int32v( Primes::W ) ) ), dx, dy, dz, dw, falloff, valueX, valueY, valueZ, valueW );
        };

        applyRankedVertex( 2, float( kUnskew4 ) );
        applyRankedVertex( 1, float( kUnskew4 * 2 ) );
        applyRankedVertex( 0, float( kUnskew4 * 3 ) );

        {
            float32v falloff = FS::Max( falloff4, float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;

            ApplyVectorContributionSimplex<Scheme>( HashPrimes( seed,
                xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ), wPrimedBase + int32v( Primes::W ) ),
                dx0 - float32v( kUnskew4 * 4 + 1 ), dy0 - float32v( kUnskew4 * 4 + 1 ), dz0 - float32v( kUnskew4 * 4 + 1 ), dw0 - float32v( kUnskew4 * 4 + 1 ),
                falloff, valueX, valueY, valueZ, valueW );
        }

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
    FASTNOISE_IMPL_GEN_FN( template WarpGenT<DispatchClass> );

public:
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
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
    // Vertex direction is chosen by the sign bit of signValue, blendv only reads the high bit so no compare is needed
    template<typename T>
    FS_FORCEINLINE static T SelectSign( float32v signValue, T ifNegative, T ifPositive )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Select( signValue < float32v( 0 ), ifNegative, ifPositive );
        }
        else
        {
            return FS::SelectHighBit( signValue, ifNegative, ifPositive );
        }
    }

    FS_FORCEINLINE static float32v SignBit( float32v signValue )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Masked( signValue < float32v( 0 ), float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
        }
        else
        {
            return signValue & FS::Cast<float>( int32v( 1 << 31 ) );
        }
    }

    // True if any lane of the clamped falloff is in range. Out of range lanes are exactly +0, so on SSE4.1/AVX2 OR the
    // halves and test for any set bit instead of compare + movemask per half
    template<typename F>
    FS_FORCEINLINE static bool AnyInRange( F falloffBase )
    {
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            auto bits = _mm256_castps_si256( _mm256_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm256_testz_si256( bits, bits );
        }
        else if constexpr( ( SIMD & FastSIMD::FeatureFlag::SSE41 ) && !( SIMD & FastSIMD::FeatureFlag::AVX ) )
        {
            auto bits = _mm_castps_si128( _mm_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm_testz_si128( bits, bits );
        }
        else
        {
            return FS::AnyMask( falloffBase != F( 0 ) );
        }
    }

    template<VectorizationScheme Scheme>
    FS_FORCEINLINE float32v Warp_2D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
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
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_3D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
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

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis direction
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase;
        int32v xPrimedDeltaPos = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v xPrimedDeltaNeg = xPrimedBase ^ ( xPrimedBase - int32v( Primes::X ) );
        int32v yPrimedDeltaPos = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v yPrimedDeltaNeg = yPrimedBase ^ ( yPrimedBase - int32v( Primes::Y ) );
        int32v zPrimedDeltaPos = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v zPrimedDeltaNeg = zPrimedBase ^ ( zPrimedBase - int32v( Primes::Z ) );

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
            float32v signValue = xyzNormal;

            float32v sign = SignBit( signValue );
            float32v offset = float32v( 3 * kReflectUnskew3 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, xDelta, yDelta, zDelta ), dxBase - offset, dyBase - offset, dzBase - offset,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 1, 0> or <-1, -1, 0>
        {
            float32v signValue = xyzNormal - zNormal; // Sign bit set when xyzNormal < zNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dzBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, xDelta, yDelta ), dxBase, dyBase, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 0, 1> or <-1, 0, -1>
        {
            float32v signValue = xyzNormal - yNormal; // Sign bit set when xyzNormal < yNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dyBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, xDelta, zDelta ), dxBase, dyBase - offset0, dzBase,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 1, 1> or <0, -1, -1>
        {
            float32v signValue = xyzNormal - xNormal; // Sign bit set when xyzNormal < xNormal

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dxBase ) - falloffBaseStemB, float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase, dzBase,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <1, 0, 0> or <-1, 0, 0>
        {
            float32v signValue = xNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, xDelta ), dxBase + offset0, dyBase - offset0, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 1, 0> or <0, -1, 0>
        {
            float32v signValue = yNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, yDelta ), dxBase - offset0, dyBase + offset0, dzBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
        }

        // Vertex <0, 0, 1> or <0, 0, -1>
        {
            float32v signValue = zNormal;

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionCommon<Scheme>( HashPrimes( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase + offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ );
            }
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
    FN_ALIGN_STACK float32v FS_VECTORCALL Warp_4D( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
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

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis direction
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase ^ wPrimedBase;
        int32v xPrimedDeltaPos = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v xPrimedDeltaNeg = xPrimedBase ^ ( xPrimedBase - int32v( Primes::X ) );
        int32v yPrimedDeltaPos = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v yPrimedDeltaNeg = yPrimedBase ^ ( yPrimedBase - int32v( Primes::Y ) );
        int32v zPrimedDeltaPos = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v zPrimedDeltaNeg = zPrimedBase ^ ( zPrimedBase - int32v( Primes::Z ) );
        int32v wPrimedDeltaPos = wPrimedBase ^ ( wPrimedBase + int32v( Primes::W ) );
        int32v wPrimedDeltaNeg = wPrimedBase ^ ( wPrimedBase - int32v( Primes::W ) );

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
            float32v signValue = xyzwNormal;
            float32v sign = SignBit( signValue );

            float32v offset = float32v( 4 * kUnskew4 + 1 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, yDelta, zDelta, wDelta ), dxBase - offset, dyBase - offset, dzBase - offset, dwBase - offset,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 1, 0> or <-1, -1, -1, 0>
        {
            float32v signValue = xyzwNormal - wNormal; // Sign bit set when xyzwNormal < wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dwBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, yDelta, zDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 0, 1> or <-1, -1, 0, -1>
        {
            float32v signValue = xyzwNormal - zNormal; // Sign bit set when xyzwNormal < zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, yDelta, wDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 1, 1> or <-1, 0, -1, -1>
        {
            float32v signValue = xyzwNormal - yNormal; // Sign bit set when xyzwNormal < yNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, zDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 1, 1> or <0, -1, -1, -1>
        {
            float32v signValue = xyzwNormal - xNormal; // Sign bit set when xyzwNormal < xNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 3 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 3 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, yDelta, zDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 0, 0> or <-1, 0, 0, 0>
        {
            float32v signValue = xNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 1, 0, 0> or <-1, -1, 0, 0>
        {
            float32v signValue = xNormal - -yNormal; // Sign bit set when xNormal < -yNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dyBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, yDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 1, 0> or <-1, 0, -1, 0>
        {
            float32v signValue = xNormal - -zNormal; // Sign bit set when xNormal < -zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dzBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, zDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <1, 0, 0, 1> or <-1, 0, 0, -1>
        {
            float32v signValue = xNormal - -wNormal; // Sign bit set when xNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, xDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 0, 0> or <0, -1, 0, 0>
        {
            float32v signValue = yNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, yDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 1, 0> or <0, -1, -1, 0>
        {
            float32v signValue = yNormal - -zNormal; // Sign bit set when yNormal < -zNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dzBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 1, 0, 1> or <0, -1, 0, -1>
        {
            float32v signValue = yNormal - -wNormal; // Sign bit set when yNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, yDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 1, 0> or <0, 0, -1, 0>
        {
            float32v signValue = zNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset0,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 1, 1> or <0, 0, -1, -1>
        {
            float32v signValue = zNormal - -wNormal; // Sign bit set when zNormal < -wNormal
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dzBase + dwBase ) ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, zDelta, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
        }

        // Vertex <0, 0, 0, 1> or <0, 0, 0, -1>
        {
            float32v signValue = wNormal;
            float32v sign = SignBit( signValue );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dwBase ), float32v( 0.0f ) );

            if( AnyInRange( falloffBase ) )
            {
                int32v wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                ApplyVectorContributionSimplex<Scheme>( HashPrimes( hashBase, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset0, dwBase - offset1,
                    ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), valueX, valueY, valueZ, valueW );
            }
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
