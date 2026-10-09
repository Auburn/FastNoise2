#include "Simplex.h"
#include "Utils.inl"

// Realign the stack on entry: clang otherwise spills AVX registers to slots that are only 16 byte aligned (Win64 ABI),
// so depending on the caller's stack alignment most 32/64 byte spills and reloads split cache lines
#ifndef FN_SIMPLEX_ALIGN_STACK
#if defined( __AVX__ ) && ( defined( __clang__ ) || defined( __GNUC__ ) )
#define FN_SIMPLEX_ALIGN_STACK __attribute__(( force_align_arg_pointer ))
#else
#define FN_SIMPLEX_ALIGN_STACK
#endif
#endif

namespace FastNoise
{
    // Variants of the Utils.inl hash/gradient helpers that accept any register width, used to evaluate each native half of
    // float32v separately where the live values don't fit in registers
    template<typename I, typename... P>
    FS_FORCEINLINE static I HashPrimesAnyWidth( I seed, P... primedPos )
    {
        I hash = seed;
        hash ^= ( primedPos ^ ... );

        hash *= I( HashMultiplier::A );

        return ( hash >> 15 ) ^ hash;
    }

    template<typename F, typename I>
    FS_FORCEINLINE static F GetGradientDotCommonAnyWidth( I hash, F fX, F fY, F fZ )
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
    FS_FORCEINLINE static F GetGradientDotPerlinAnyWidth( I hash, F fX, F fY, F fZ, F fW )
    {
        if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX512_F )
        {
            F gX = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 ), hash, FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 ) );
            F gY = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ), hash, FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ) );
            F gZ = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 ), hash, FS::Constant<float>( 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, -1, -1, -1, -1 ) );
            F gW = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm512_permutex2var_ps ), FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1, 1, -1, -1, -1, -1 ), hash, FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0 ) );

            return FS::FMulAdd( gX, fX, FS::FMulAdd( fY, gY, FS::FMulAdd( fZ, gZ, fW * gW ) ));
        }
        else if constexpr( F::FeatureFlags & FastSIMD::FeatureFlag::AVX2 )
        {
            I p = hash & I( 3 << 3 );

            F a = FS::Select( p > I( 0 ), fX, fY );
            F b = FS::SelectHighBit( hash << 27, fY, fZ );
            F c = FS::Select( p > I( 2 << 3 ), fZ, fW );

            F aSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, -1, 1, -1, 1, -1, 1, -1 ), hash );
            F bSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar_ps ), FS::Constant<float>( 1, 1, -1, -1, 1, 1, -1, -1 ), hash );
            F cSign = FS::NativeExec<F>( FS_BIND_INTRINSIC( _mm256_permutevar8x32_ps ), FS::Constant<float>( 1, 1, 1, 1, -1, -1, -1, -1 ), hash );

            return FS::FMulAdd( c, cSign, FS::FMulAdd( b, bSign, a * aSign ) );
        }
        else
        {
            I p = hash & I( 3 << 3 );

            F a = FS::Select( p > I( 0 ), fX, fY );
            F b = FS::SelectHighBit( hash << 27, fY, fZ );
            F c = FS::Select( p > I( 2 << 3 ), fZ, fW );

            F aSign = FS::Cast<float>( hash << 31 );
            F bSign = FS::Cast<float>( ( hash >> 1 ) << 31 );
            F cSign = FS::Cast<float>( ( hash >> 2 ) << 31 );

            return ( a ^ aSign ) + ( b ^ bSign ) + ( c ^ cSign );
        }
    }
}

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Simplex, SIMD> final : public virtual Simplex, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y );

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

        float32v gradientRampValue0 = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase ), dx0, dy0 );
        float32v gradientRampValue1 = GetGradientDotPerlin( HashPrimes( seed, FS::MaskedAdd( xGreaterEqualY, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( xGreaterEqualY, yPrimedBase, int32v( Primes::Y ) ) ), dx1, dy1 );
        float32v gradientRampValue2 = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) ), dx2, dy2 );

        float32v value = FS::FMulAdd( gradientRampValue0, falloff0, FS::FMulAdd( gradientRampValue1, falloff1, gradientRampValue2 * falloff2 ) );

        constexpr float kBounding = 0.0261208079755306243896484375f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z );

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

        float32v gradientRampValue0 = GetGradientDotCommonAnyWidth( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dx0, dy0, dz0 );
        float32v gradientRampValue1 = GetGradientDotCommonAnyWidth( HashPrimes( seed, FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ), FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ) ), dx1, dy1, dz1 );
        float32v gradientRampValue2 = GetGradientDotCommonAnyWidth( HashPrimes( seed, FS::MaskedAdd( nMaskX2, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( nMaskY2, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( nMaskZ2, zPrimedBase, int32v( Primes::Z ) ) ), dx2, dy2, dz2 );
        float32v gradientRampValue3 = GetGradientDotCommonAnyWidth( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ) ), dx3, dy3, dz3 );

        float32v value = FS::FMulAdd( gradientRampValue3, falloff3, FS::FMulAdd( gradientRampValue2, falloff2, FS::FMulAdd( gradientRampValue1, falloff1, gradientRampValue0 * falloff0 ) ) );

        constexpr float kBounding = 0.030586399137973785400390625f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z, w );

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
        rankX = FS::MaskedIncrement(  xGreaterEqualY, rankX );
        rankY = FS::MaskedIncrement( ~xGreaterEqualY, rankY );

        mask32v xGreaterEqualZ = dx0 >= dz0;
        rankX = FS::MaskedIncrement(  xGreaterEqualZ, rankX );
        rankZ = FS::MaskedIncrement( ~xGreaterEqualZ, rankZ );

        mask32v xGreaterEqualW = dx0 >= dw0;
        rankX = FS::MaskedIncrement(  xGreaterEqualW, rankX );
        rankW = FS::MaskedIncrement( ~xGreaterEqualW, rankW );

        mask32v yGreaterEqualZ = dy0 >= dz0;
        rankY = FS::MaskedIncrement(  yGreaterEqualZ, rankY );
        rankZ = FS::MaskedIncrement( ~yGreaterEqualZ, rankZ );

        mask32v yGreaterEqualW = dy0 >= dw0;
        rankY = FS::MaskedIncrement(  yGreaterEqualW, rankY );
        rankW = FS::MaskedIncrement( ~yGreaterEqualW, rankW );

        mask32v zGreaterEqualW = dz0 >= dw0;
        rankZ = FS::MaskedIncrement(  zGreaterEqualW, rankZ );
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

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase ^ wPrimedBase;
        int32v xPrimedDelta = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v yPrimedDelta = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v zPrimedDelta = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v wPrimedDelta = wPrimedBase ^ ( wPrimedBase + int32v( Primes::W ) );

        float32v gradientRampValue0 = GetGradientDotPerlinAnyWidth( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0 );
        float32v gradientRampValue1 = GetGradientDotPerlinAnyWidth( HashPrimes( hashBase,
            FS::Masked( maskX1, xPrimedDelta ),
            FS::Masked( maskY1, yPrimedDelta ),
            FS::Masked( maskZ1, zPrimedDelta ),
            FS::Masked( maskW1, wPrimedDelta ) ), dx1, dy1, dz1, dw1 );
        float32v gradientRampValue2 = GetGradientDotPerlinAnyWidth( HashPrimes( hashBase,
            FS::Masked( maskX2, xPrimedDelta ),
            FS::Masked( maskY2, yPrimedDelta ),
            FS::Masked( maskZ2, zPrimedDelta ),
            FS::Masked( maskW2, wPrimedDelta ) ), dx2, dy2, dz2, dw2 );
        float32v gradientRampValue3 = GetGradientDotPerlinAnyWidth( HashPrimes( hashBase,
            FS::Masked( maskX3, xPrimedDelta ),
            FS::Masked( maskY3, yPrimedDelta ),
            FS::Masked( maskZ3, zPrimedDelta ),
            FS::Masked( maskW3, wPrimedDelta ) ), dx3, dy3, dz3, dw3 );
        float32v gradientRampValue4 = GetGradientDotPerlinAnyWidth( HashPrimes( hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, wPrimedDelta ), dx4, dy4, dz4, dw4 );

        float32v value = FS::FMulAdd( gradientRampValue0, falloff0, FS::FMulAdd( gradientRampValue1, falloff1, FS::FMulAdd( gradientRampValue2, falloff2, FS::FMulAdd( gradientRampValue3, falloff3, gradientRampValue4 * falloff4 ) ) ) );

        constexpr float kBounding = 0.036730043590068817138671875f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<SuperSimplex, SIMD> final : public virtual SuperSimplex, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    // Vertex direction is chosen by the sign bit of signValue, blendv only reads the high bit so no compare is needed
    template<typename F, typename T>
    FS_FORCEINLINE static T SelectSign( F signValue, T ifNegative, T ifPositive )
    {
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Select( signValue < F( 0 ), ifNegative, ifPositive );
        }
        else
        {
            return FS::SelectHighBit( signValue, ifNegative, ifPositive );
        }
    }

    // Skipping vertices that are out of range for every point pays off when SIMD lanes are spatially coherent.
    // 3D on AVX512 (32 points per register) rarely skips enough to cover the extra branching, 4D on AVX512 is
    // evaluated per 16 point half and always skips
    static constexpr bool kSkipOutOfRangeVertices3D = !( SIMD & FastSIMD::FeatureFlag::AVX512_F );

    // True if any lane of the clamped falloff is in range. Out of range lanes are exactly +0, so on SSE4.1/AVX2 OR the
    // halves and test for any set bit instead of compare + movemask per half
    template<typename F>
    FS_FORCEINLINE static bool AnyInRange( F falloffBase, typename F::MaskType inRangeMask )
    {
        if constexpr( !FS::IsNativeV<F> && ( SIMD & FastSIMD::FeatureFlag::AVX2 ) && !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            auto bits = _mm256_castps_si256( _mm256_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm256_testz_si256( bits, bits );
        }
        else if constexpr( !FS::IsNativeV<F> && ( SIMD & FastSIMD::FeatureFlag::SSE41 ) && !( SIMD & FastSIMD::FeatureFlag::AVX ) )
        {
            auto bits = _mm_castps_si128( _mm_or_ps( falloffBase.v0.GetNative(), falloffBase.v1.GetNative() ) );
            return !_mm_testz_si128( bits, bits );
        }
        else
        {
            return FS::AnyMask( inRangeMask );
        }
    }

    template<typename F>
    FS_FORCEINLINE static F SignBit( F signValue )
    {
        auto signBit = FS::Cast<float>( decltype( FS::Cast<std::int32_t>( signValue ) )( 1 << 31 ) );

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            return FS::Masked( signValue < F( 0 ), signBit );
        }
        else
        {
            return signValue & signBit;
        }
    }

    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y );

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

        float32v falloffBase0, value;

        // Vertex <0, 0>
        {
            int32v hash = HashPrimes( seed, xPrimedBase, yPrimedBase );
            float32v gradientRampValue = GetGradientDotPerlin( hash, dxBase, dyBase );
            falloffBase0 = FS::FNMulAdd( dxBase, dxBase, FS::FNMulAdd( dyBase, dyBase, float32v( kFalloffRadiusSquared ) ) );
            float32v falloff = falloffBase0; falloff *= falloff; falloff *= falloff;
            value = falloff * gradientRampValue;
        }

        // Vertex <1, 1>
        {
            int32v hash = HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) );
            float32v gradientRampValue = GetGradientDotPerlin( hash, dxBase - float32v( 2 * kUnskew2 + 1 ), dyBase - float32v( 2 * kUnskew2 + 1 ) );
            float32v falloff = FS::FMulAdd( unskewDelta,
                float32v( -4.0 * ( kRoot3 + 2.0 ) / ( kRoot3 + 3.0 ) ),
                falloffBase0 - float32v( kFalloffRadiusSquared ) );
            falloff *= falloff; falloff *= falloff;
            value = FS::FMulAdd( falloff, gradientRampValue, value );
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
            float32v gradientRampValue = GetGradientDotPerlin( hash, dx, dy );
            float32v falloff = FS::Max( FS::FNMulAdd( dx, dx, FS::FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            value = FS::FMulAdd( falloff, gradientRampValue, value );
        }

        // Vertex <0, 1> or <0, -1> or <2, 1>
        {
            int32v hash = HashPrimes( seed,
                FS::MaskedAdd( forwardXY, xPrimedBase, int32v( Primes::X ) ),
                FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardY, yPrimedBase, int32v( (int32_t)( Primes::Y * 2LL ) ) ), int32v( Primes::Y ) ) );
            float32v dx = FS::MaskedSub( forwardY, dxBase, float32v( 2 * kUnskew2 ) );
            float32v dy = dyBase - FS::Select( forwardY, float32v( 1 + 2 * kUnskew2 ), float32v( -1 ) );
            float32v gradientRampValue = GetGradientDotPerlin( hash, dx, dy );
            float32v falloff = FS::Max( FS::FNMulAdd( dx, dx, FS::FNMulAdd( dy, dy, float32v( kFalloffRadiusSquared ) ) ), float32v( 0 ) );
            falloff *= falloff; falloff *= falloff;
            value = FS::FMulAdd( falloff, gradientRampValue, value );
        }

        constexpr float kBounding = 0.14084912836551666259765625f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z );

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

        // Vertex <0, 0, 0>
        float32v value, falloffBaseStemA, falloffBaseStemB;
        {
            float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dxBase, dyBase, dzBase );
            float32v falloffBase = FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            value = ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ) * gradientRampValue;
        }

        // Vertex <1, 1, 1> or <-1, -1, -1>
        {
            float32v signValue = xyzNormal;

            int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
            int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
            int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

            float32v sign = SignBit( signValue );
            float32v offset = float32v( 3 * kReflectUnskew3 + 1 ) ^ sign;

            float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, xDelta, yDelta, zDelta ), dxBase - offset, dyBase - offset, dzBase - offset );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 1, 0> or <-1, -1, 0>
        {
            float32v signValue = xyzNormal - zNormal; // Sign bit set when xyzNormal < zNormal
            float32v sign = SignBit( signValue );

            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dzBase ) - falloffBaseStemB, float32v( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( !kSkipOutOfRangeVertices3D || AnyInRange( falloffBase, falloffBase < float32v( 0 ) ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, xDelta, yDelta ), dxBase, dyBase, dzBase - offset0 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 1> or <-1, 0, -1>
        {
            float32v signValue = xyzNormal - yNormal; // Sign bit set when xyzNormal < yNormal
            float32v sign = SignBit( signValue );

            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dyBase ) - falloffBaseStemB, float32v( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( !kSkipOutOfRangeVertices3D || AnyInRange( falloffBase, falloffBase < float32v( 0 ) ) )
            {
                int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, xDelta, zDelta ), dxBase, dyBase - offset0, dzBase );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 1, 1> or <0, -1, -1>
        {
            float32v signValue = xyzNormal - xNormal; // Sign bit set when xyzNormal < xNormal
            float32v sign = SignBit( signValue );

            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v falloffBase = FS::Min( ( sign ^ dxBase ) - falloffBaseStemB, float32v( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( !kSkipOutOfRangeVertices3D || AnyInRange( falloffBase, falloffBase < float32v( 0 ) ) )
            {
                int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase, dzBase );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 0> or <-1, 0, 0>
        {
            float32v signValue = xNormal;

            int32v xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, xDelta ), dxBase + offset0, dyBase - offset0, dzBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 0> or <0, -1, 0>
        {
            float32v signValue = yNormal;

            int32v yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, yDelta ), dxBase - offset0, dyBase + offset0, dzBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 1> or <0, 0, -1>
        {
            float32v signValue = zNormal;

            int32v zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

            float32v sign = SignBit( signValue );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommonAnyWidth( HashPrimes( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase + offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        constexpr float kBounding = 0.0069091119803488254547119140625f;
        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    // The 4D vertex loop has more live values than there are registers, on AVX512 evaluating each native half of the 2x
    // wide float32v separately avoids most spilling. SSE/AVX2 still spill when split and lose more from the vertex
    // skipping branches being per half, so they evaluate the full width
    template<typename F, typename I>
    FS_FORCEINLINE static F GenSuperSimplex4D( I seed, F x, F y, F z, F w )
    {
        using M = typename F::MaskType;

        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kTwiceUnskew4 = -1.0 / 5.0;

        constexpr double kDistanceSquaredA = 4.0 / 5.0;
        constexpr double kDistanceSquaredB = 6.0 / 5.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        F skewDelta = F( kSkew4 ) * ( x + y + z + w );

        F xSkewed = x + skewDelta;
        F ySkewed = y + skewDelta;
        F zSkewed = z + skewDelta;
        F wSkewed = w + skewDelta;
        F xSkewedBase = FS::Floor( xSkewed );
        F ySkewedBase = FS::Floor( ySkewed );
        F zSkewedBase = FS::Floor( zSkewed );
        F wSkewedBase = FS::Floor( wSkewed );
        F dxSkewed = xSkewed - xSkewedBase;
        F dySkewed = ySkewed - ySkewedBase;
        F dzSkewed = zSkewed - zSkewedBase;
        F dwSkewed = wSkewed - wSkewedBase;

        // From unit cell base, find closest vertex
        {
            // Perform a double unskew to get the vector whose dot product with skewed vectors produces the unskewed result.
            F twiceUnskewDelta = F( kTwiceUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
            F xNormal = dxSkewed + twiceUnskewDelta;
            F yNormal = dySkewed + twiceUnskewDelta;
            F zNormal = dzSkewed + twiceUnskewDelta;
            F wNormal = dwSkewed + twiceUnskewDelta;
            F xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

            // Using those, compare scores to determine which vertex is closest.
            constexpr auto considerVertex = [] ( F& maxScore, I& moveMaskBits, F score, I bits ) constexpr
            {
                moveMaskBits = FS::Select( score > maxScore, bits, moveMaskBits );
                maxScore = FS::Max( maxScore, score );
            };
            // Candidates sharing a score offset are first reduced in independent pairs to shorten the dependency chain,
            // max and "first strictly greater candidate wins" are associative so the result is unchanged
            F maxScore = F( 0.6f ) - xyzwNormal;
            I moveMaskBits = FS::Masked( F( 0.2f ) > maxScore, I( -1 ) );
            maxScore = FS::Max( maxScore, F( 0.2f ) );

            F maxScore3A = -wNormal;
            I moveMaskBits3A = I( 0b0111 );
            considerVertex( maxScore3A, moveMaskBits3A, -zNormal, 0b1011 );
            F maxScore3B = -yNormal;
            I moveMaskBits3B = I( 0b1101 );
            considerVertex( maxScore3B, moveMaskBits3B, -xNormal, 0b1110 );
            considerVertex( maxScore, moveMaskBits, maxScore3A, moveMaskBits3A );
            considerVertex( maxScore, moveMaskBits, maxScore3B, moveMaskBits3B );

            F maxScore1A = xNormal;
            I moveMaskBits1A = I( 0b0001 );
            considerVertex( maxScore1A, moveMaskBits1A, yNormal, 0b0010 );
            F maxScore1B = zNormal;
            I moveMaskBits1B = I( 0b0100 );
            considerVertex( maxScore1B, moveMaskBits1B, wNormal, 0b1000 );
            considerVertex( maxScore1A, moveMaskBits1A, maxScore1B, moveMaskBits1B );

            F maxScore2X = yNormal;
            I moveMaskBits2X = I( 0b0011 );
            considerVertex( maxScore2X, moveMaskBits2X, zNormal, 0b0101 );
            considerVertex( maxScore2X, moveMaskBits2X, wNormal, 0b1001 );

            F maxScore2W = yNormal;
            I moveMaskBits2W = I( 0b1010 );
            considerVertex( maxScore2W, moveMaskBits2W, zNormal, 0b1100 );

            maxScore += xyzwNormal - F( 0.2f );
            considerVertex( maxScore, moveMaskBits, maxScore1A, moveMaskBits1A );
            maxScore += F( 0.2f ) - xNormal;
            considerVertex( maxScore, moveMaskBits, maxScore2X, moveMaskBits2X );
            maxScore += xNormal;
            considerVertex( maxScore, moveMaskBits, yNormal + zNormal, 0b0110 );
            maxScore -= wNormal;
            considerVertex( maxScore, moveMaskBits, maxScore2W, moveMaskBits2W );

            M moveX = ( moveMaskBits & I( 0b0001 ) ) != I( 0 );
            M moveY = ( moveMaskBits & I( 0b0010 ) ) != I( 0 );
            M moveZ = ( moveMaskBits & I( 0b0100 ) ) != I( 0 );
            M moveW = ( moveMaskBits & I( 0b1000 ) ) != I( 0 );

            xSkewedBase = FS::MaskedIncrement( moveX, xSkewedBase );
            ySkewedBase = FS::MaskedIncrement( moveY, ySkewedBase );
            zSkewedBase = FS::MaskedIncrement( moveZ, zSkewedBase );
            wSkewedBase = FS::MaskedIncrement( moveW, wSkewedBase );

            dxSkewed = FS::MaskedDecrement( moveX, dxSkewed );
            dySkewed = FS::MaskedDecrement( moveY, dySkewed );
            dzSkewed = FS::MaskedDecrement( moveZ, dzSkewed );
            dwSkewed = FS::MaskedDecrement( moveW, dwSkewed );
        }

        I xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * I( Primes::X );
        I yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * I( Primes::Y );
        I zPrimedBase = FS::Convert<int32_t>( zSkewedBase ) * I( Primes::Z );
        I wPrimedBase = FS::Convert<int32_t>( wSkewedBase ) * I( Primes::W );

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis direction
        I hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase ^ wPrimedBase;
        I xPrimedDeltaPos = xPrimedBase ^ ( xPrimedBase + I( Primes::X ) );
        I xPrimedDeltaNeg = xPrimedBase ^ ( xPrimedBase - I( Primes::X ) );
        I yPrimedDeltaPos = yPrimedBase ^ ( yPrimedBase + I( Primes::Y ) );
        I yPrimedDeltaNeg = yPrimedBase ^ ( yPrimedBase - I( Primes::Y ) );
        I zPrimedDeltaPos = zPrimedBase ^ ( zPrimedBase + I( Primes::Z ) );
        I zPrimedDeltaNeg = zPrimedBase ^ ( zPrimedBase - I( Primes::Z ) );
        I wPrimedDeltaPos = wPrimedBase ^ ( wPrimedBase + I( Primes::W ) );
        I wPrimedDeltaNeg = wPrimedBase ^ ( wPrimedBase - I( Primes::W ) );

        F skewedCoordinateSum = dxSkewed + dySkewed + dzSkewed + dwSkewed;
        F twiceUnskewDelta = F( kTwiceUnskew4 ) * skewedCoordinateSum;
        F xNormal = dxSkewed + twiceUnskewDelta;
        F yNormal = dySkewed + twiceUnskewDelta;
        F zNormal = dzSkewed + twiceUnskewDelta;
        F wNormal = dwSkewed + twiceUnskewDelta;
        F xyzwNormal = -twiceUnskewDelta; // xNormal + yNormal + zNormal + wNormal

        F unskewDelta = F( kUnskew4 ) * skewedCoordinateSum;
        F dxBase = dxSkewed + unskewDelta;
        F dyBase = dySkewed + unskewDelta;
        F dzBase = dzSkewed + unskewDelta;
        F dwBase = dwSkewed + unskewDelta;

        F coordinateSum = F( 1 + 4 * kUnskew4 ) * skewedCoordinateSum; // dxBase + dyBase + dzBase + dwBase

        // Vertex <0, 0, 0, 0>
        F value, falloffBaseStemA, falloffBaseStemB;
        {
            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase, dyBase, dzBase, dwBase );
            F falloffBase = FS::FNMulAdd( dwBase, dwBase, FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, F( kFalloffRadiusSquared ) ) ) ) ) * F( 0.5f );
            falloffBaseStemA = falloffBase - F( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - F( kDistanceSquaredB * 0.5 );
            value = ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ) * gradientRampValue;
        }

        // Vertex <1, 1, 1, 1> or <-1, -1, -1, -1>
        {
            F signValue = xyzwNormal;
            F sign = SignBit( signValue );

            I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
            I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
            I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
            I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

            F offset = F( 4 * kUnskew4 + 1 ) ^ sign;

            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, yDelta, zDelta, wDelta ), dxBase - offset, dyBase - offset, dzBase - offset, dwBase - offset );
            F falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), F( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 1, 1, 0> or <-1, -1, -1, 0>
        {
            F signValue = xyzwNormal - wNormal; // Sign bit set when xyzwNormal < wNormal
            F sign = SignBit( signValue );

            F offset1 = F( 3 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 3 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dwBase ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, yDelta, zDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset1, dwBase - offset0 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 1, 0, 1> or <-1, -1, 0, -1>
        {
            F signValue = xyzwNormal - zNormal; // Sign bit set when xyzwNormal < zNormal
            F sign = SignBit( signValue );

            F offset1 = F( 3 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 3 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dzBase ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, yDelta, wDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 1, 1> or <-1, 0, -1, -1>
        {
            F signValue = xyzwNormal - yNormal; // Sign bit set when xyzwNormal < yNormal
            F sign = SignBit( signValue );

            F offset1 = F( 3 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 3 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dyBase ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, zDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 1, 1, 1> or <0, -1, -1, -1>
        {
            F signValue = xyzwNormal - xNormal; // Sign bit set when xyzwNormal < xNormal
            F sign = SignBit( signValue );

            F offset1 = F( 3 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 3 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dxBase ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, yDelta, zDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 0, 0> or <-1, 0, 0, 0>
        {
            F signValue = xNormal;
            F sign = SignBit( signValue );

            I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );

            F offset1 = F( kUnskew4 + 1 ) ^ sign;
            F offset0 = F( kUnskew4 ) ^ sign;

            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset0 );
            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), F( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 1, 0, 0> or <-1, -1, 0, 0>
        {
            F signValue = xNormal + yNormal; // Sign bit set when xNormal < -yNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dyBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, yDelta ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset0 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 1, 0> or <-1, 0, -1, 0>
        {
            F signValue = xNormal + zNormal; // Sign bit set when xNormal < -zNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dzBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, zDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset0 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <1, 0, 0, 1> or <-1, 0, 0, -1>
        {
            F signValue = xNormal + wNormal; // Sign bit set when xNormal < -wNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dwBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I xDelta = SelectSign( signValue, xPrimedDeltaNeg, xPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, xDelta, wDelta ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 1, 0, 0> or <0, -1, 0, 0>
        {
            F signValue = yNormal;
            F sign = SignBit( signValue );

            I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );

            F offset1 = F( kUnskew4 + 1 ) ^ sign;
            F offset0 = F( kUnskew4 ) ^ sign;

            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, yDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset0 );
            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), F( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 1, 0> or <0, -1, -1, 0>
        {
            F signValue = yNormal + zNormal; // Sign bit set when yNormal < -zNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dzBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, yDelta, zDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset0 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 1, 0, 1> or <0, -1, 0, -1>
        {
            F signValue = yNormal + wNormal; // Sign bit set when yNormal < -wNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dwBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I yDelta = SelectSign( signValue, yPrimedDeltaNeg, yPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, yDelta, wDelta ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 0, 1, 0> or <0, 0, -1, 0>
        {
            F signValue = zNormal;
            F sign = SignBit( signValue );

            I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );

            F offset1 = F( kUnskew4 + 1 ) ^ sign;
            F offset0 = F( kUnskew4 ) ^ sign;

            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, zDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset0 );
            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), F( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 1, 1> or <0, 0, -1, -1>
        {
            F signValue = zNormal + wNormal; // Sign bit set when zNormal < -wNormal
            F sign = SignBit( signValue );

            F offset1 = F( 2 * kUnskew4 + 1 ) ^ sign;
            F offset0 = F( 2 * kUnskew4 ) ^ sign;

            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dzBase + dwBase ) ), F( 0.0f ) );

            // Skip the vertex when it is out of range for every point
            if( AnyInRange( falloffBase, falloffBase > F( 0 ) ) )
            {
                I zDelta = SelectSign( signValue, zPrimedDeltaNeg, zPrimedDeltaPos );
                I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

                F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, zDelta, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset1 );
                value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
            }
        }

        // Vertex <0, 0, 0, 1> or <0, 0, 0, -1>
        {
            F signValue = wNormal;
            F sign = SignBit( signValue );

            I wDelta = SelectSign( signValue, wPrimedDeltaNeg, wPrimedDeltaPos );

            F offset1 = F( kUnskew4 + 1 ) ^ sign;
            F offset0 = F( kUnskew4 ) ^ sign;

            F gradientRampValue = GetGradientDotPerlinAnyWidth( HashPrimesAnyWidth( hashBase, wDelta ), dxBase - offset0, dyBase - offset0, dzBase - offset0, dwBase - offset1 );
            F falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dwBase ), F( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        return value;
    }

    FN_SIMPLEX_ALIGN_STACK float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z, w );

        float32v value;
        if constexpr( FS::IsNativeV<float32v> || !( SIMD & FastSIMD::FeatureFlag::AVX512_F ) )
        {
            value = GenSuperSimplex4D( seed, x, y, z, w );
        }
        else
        {
            value = float32v{ GenSuperSimplex4D( seed.v0, x.v0, y.v0, z.v0, w.v0 ), GenSuperSimplex4D( seed.v1, x.v1, y.v1, z.v1, w.v1 ) };
        }

        constexpr float kBounding = 0.011310013942420482635498046875;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

};
