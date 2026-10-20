#include "Simplex.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<Simplex, SIMD> final : public virtual Simplex, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    // d - ( moved ? unskew + 1 : unskew ). Selecting between the two constants is a single blend, SSE2 has no blendv so
    // it masks the 1 instead
    FS_FORCEINLINE static float32v OffsetFromVertex( mask32v moved, float32v d, double unskew )
    {
        if constexpr( ( SIMD & FastSIMD::FeatureFlag::x86 ) && !( SIMD & FastSIMD::FeatureFlag::SSE41 ) )
        {
            return FS::MaskedSub( moved, d, float32v( 1 ) ) - float32v( (float)unskew );
        }
        else
        {
            return d - FS::Select( moved, float32v( (float)( unskew + 1 ) ), float32v( (float)unskew ) );
        }
    }

    // Offsets, falloff and gradient of one 4D vertex. Evaluating each vertex in one go keeps fewer offsets and masks live
    // at once, mostly helps SSE where float32v only has 8 registers. (As a lambda MSVC miscompiled it for AVX512)
    FS_FORCEINLINE static float32v VertexGradientRampValue( float32v& falloff, double unskew, mask32v maskX, mask32v maskY, mask32v maskZ, mask32v maskW,
        float32v dx0, float32v dy0, float32v dz0, float32v dw0, int32v hashBase, int32v xPrimedDelta, int32v yPrimedDelta, int32v zPrimedDelta, int32v wPrimedDelta )
    {
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v dx = OffsetFromVertex( maskX, dx0, unskew );
        float32v dy = OffsetFromVertex( maskY, dy0, unskew );
        float32v dz = OffsetFromVertex( maskZ, dz0, unskew );
        float32v dw = OffsetFromVertex( maskW, dw0, unskew );

        falloff = FS::FNMulAdd( dw, dw, FS::FNMulAdd( dz, dz, FS::FNMulAdd( dy, dy, FS::FNMulAdd( dx, dx, float32v( kFalloffRadiusSquared ) ) ) ) );
        falloff = FS::Max( falloff, float32v( 0 ) );
        falloff *= falloff; falloff *= falloff;

        return GetGradientDotPerlin( HashPrimes( hashBase,
            FS::Masked( maskX, xPrimedDelta ),
            FS::Masked( maskY, yPrimedDelta ),
            FS::Masked( maskZ, zPrimedDelta ),
            FS::Masked( maskW, wPrimedDelta ) ), dx, dy, dz, dw );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
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

        int32v xPrimedBase = FloorToInt( xSkewed, xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FloorToInt( ySkewed, ySkewedBase ) * int32v( Primes::Y );

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
        float32v gradientRampValue1, gradientRampValue2;
        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            // Masked adds are single instructions and the xors fuse into vpternlogd
            gradientRampValue1 = GetGradientDotPerlin( HashPrimes( seed, FS::MaskedAdd( xGreaterEqualY, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( xGreaterEqualY, yPrimedBase, int32v( Primes::Y ) ) ), dx1, dy1 );
            gradientRampValue2 = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) ), dx2, dy2 );
        }
        else
        {
            // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis.
            // Vertex 1 moves exactly one axis
            int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase;
            int32v xPrimedDelta = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
            int32v yPrimedDelta = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );

            gradientRampValue1 = GetGradientDotPerlin( HashPrimes( hashBase, FS::Select( xGreaterEqualY, xPrimedDelta, yPrimedDelta ) ), dx1, dy1 );
            gradientRampValue2 = GetGradientDotPerlin( HashPrimes( hashBase, xPrimedDelta, yPrimedDelta ), dx2, dy2 );
        }

        float32v value = FS::FMulAdd( gradientRampValue0, falloff0, FS::FMulAdd( gradientRampValue1, falloff1, gradientRampValue2 * falloff2 ) );

        constexpr float kBounding = 0.0261208079755306243896484375f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z );

        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kFalloffRadiusSquared = 0.6;

        float32v skewDelta = float32v( kSkew3 ) * SkewSum( x, y, z );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;
        float32v zSkewed = z + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v zSkewedBase = FS::Floor( zSkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;
        float32v dzSkewed = zSkewed - zSkewedBase;

        int32v xPrimedBase = FloorToInt( xSkewed, xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FloorToInt( ySkewed, ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FloorToInt( zSkewed, zSkewedBase ) * int32v( Primes::Z );

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

        float32v gradientRampValue0 = GetGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dx0, dy0, dz0 );

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis.
        // Vertex 1 moves only the largest axis, vertex 2 all but the smallest. On AVX512 this also measured faster than
        // masked prime adds, it needs fewer (inverted) k masks
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase;
        int32v xPrimedDelta = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v yPrimedDelta = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v zPrimedDelta = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );

        int32v largestPrimedDelta = FS::Select( maskX1, xPrimedDelta, FS::Select( maskY1, yPrimedDelta, zPrimedDelta ) );
        int32v smallestPrimedDelta = FS::Select( nMaskY2, yPrimedDelta, FS::Select( nMaskZ2, zPrimedDelta, xPrimedDelta ) );

        float32v gradientRampValue1 = GetGradientDotCommon( HashPrimes( hashBase, largestPrimedDelta ), dx1, dy1, dz1 );
        float32v gradientRampValue2 = GetGradientDotCommon( HashPrimes( hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, smallestPrimedDelta ), dx2, dy2, dz2 );
        float32v gradientRampValue3 = GetGradientDotCommon( HashPrimes( hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta ), dx3, dy3, dz3 );

        float32v value = FS::FMulAdd( gradientRampValue3, falloff3, FS::FMulAdd( gradientRampValue2, falloff2, FS::FMulAdd( gradientRampValue1, falloff1, gradientRampValue0 * falloff0 ) ) );

        constexpr float kBounding = 0.030586399137973785400390625f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
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

        int32v xPrimedBase = FloorToInt( xSkewed, xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FloorToInt( ySkewed, ySkewedBase ) * int32v( Primes::Y );
        int32v zPrimedBase = FloorToInt( zSkewed, zSkewedBase ) * int32v( Primes::Z );
        int32v wPrimedBase = FloorToInt( wSkewed, wSkewedBase ) * int32v( Primes::W );

        float32v unskewDelta = float32v( kUnskew4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
        float32v dx0 = dxSkewed + unskewDelta;
        float32v dy0 = dySkewed + unskewDelta;
        float32v dz0 = dzSkewed + unskewDelta;
        float32v dw0 = dwSkewed + unskewDelta;

        int32v rankX( 0 );
        int32v rankY( 0 );
        int32v rankZ( 0 );
        int32v rankW( 0 );

        if constexpr( SIMD & FastSIMD::FeatureFlag::AVX512_F )
        {
            // Masked increments need a mask to vector conversion each and the inverted comparisons on AVX512. Use the
            // comparisons as -1/0 integers instead, each one adds to one rank and subtracts from the other
            int32v xGreaterEqualY = FS::Masked( dx0 >= dy0, int32v( -1 ) );
            int32v xGreaterEqualZ = FS::Masked( dx0 >= dz0, int32v( -1 ) );
            int32v xGreaterEqualW = FS::Masked( dx0 >= dw0, int32v( -1 ) );
            int32v yGreaterEqualZ = FS::Masked( dy0 >= dz0, int32v( -1 ) );
            int32v yGreaterEqualW = FS::Masked( dy0 >= dw0, int32v( -1 ) );
            int32v zGreaterEqualW = FS::Masked( dz0 >= dw0, int32v( -1 ) );

            rankX = rankX - xGreaterEqualY - xGreaterEqualZ - xGreaterEqualW;
            rankY = int32v( 1 ) + xGreaterEqualY - yGreaterEqualZ - yGreaterEqualW;
            rankZ = int32v( 2 ) + xGreaterEqualZ + yGreaterEqualZ - zGreaterEqualW;
            rankW = int32v( 3 ) + xGreaterEqualW + yGreaterEqualW + zGreaterEqualW;
        }
        else
        {
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
        }

        float32v falloff0 = FS::FNMulAdd( dw0, dw0, FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( kFalloffRadiusSquared ) ) ) ) );

        // Vertex hashes only differ from the base hash on the moved axes, precompute ( base ^ moved ) for each axis
        int32v hashBase = seed ^ xPrimedBase ^ yPrimedBase ^ zPrimedBase ^ wPrimedBase;
        int32v xPrimedDelta = xPrimedBase ^ ( xPrimedBase + int32v( Primes::X ) );
        int32v yPrimedDelta = yPrimedBase ^ ( yPrimedBase + int32v( Primes::Y ) );
        int32v zPrimedDelta = zPrimedBase ^ ( zPrimedBase + int32v( Primes::Z ) );
        int32v wPrimedDelta = wPrimedBase ^ ( wPrimedBase + int32v( Primes::W ) );

        float32v falloff4 = falloff0 + FS::FMulAdd( unskewDelta,
            float32v( -4.0 * ( kRoot5 + 3.0 ) / ( kRoot5 + 5.0 ) ),
            float32v( -4.0 / 5.0 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff4 = FS::Max( falloff4, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff4 *= falloff4; falloff4 *= falloff4;

        float32v gradientRampValue0 = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0 );
        float32v gradientRampValue4 = GetGradientDotPerlin( HashPrimes( hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, wPrimedDelta ),
            dx0 - float32v( kUnskew4 * 4 + 1 ), dy0 - float32v( kUnskew4 * 4 + 1 ), dz0 - float32v( kUnskew4 * 4 + 1 ), dw0 - float32v( kUnskew4 * 4 + 1 ) );

        float32v falloff1, falloff2, falloff3;
        float32v gradientRampValue1 = VertexGradientRampValue( falloff1, kUnskew4, rankX > int32v( 2 ), rankY > int32v( 2 ), rankZ > int32v( 2 ), rankW > int32v( 2 ),
            dx0, dy0, dz0, dw0, hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, wPrimedDelta );
        float32v gradientRampValue2 = VertexGradientRampValue( falloff2, kUnskew4 * 2, rankX > int32v( 1 ), rankY > int32v( 1 ), rankZ > int32v( 1 ), rankW > int32v( 1 ),
            dx0, dy0, dz0, dw0, hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, wPrimedDelta );
        float32v gradientRampValue3 = VertexGradientRampValue( falloff3, kUnskew4 * 3, rankX > int32v( 0 ), rankY > int32v( 0 ), rankZ > int32v( 0 ), rankW > int32v( 0 ),
            dx0, dy0, dz0, dw0, hashBase, xPrimedDelta, yPrimedDelta, zPrimedDelta, wPrimedDelta );

        float32v value = FS::FMulAdd( gradientRampValue0, falloff0, FS::FMulAdd( gradientRampValue1, falloff1, FS::FMulAdd( gradientRampValue2, falloff2, FS::FMulAdd( gradientRampValue3, falloff3, gradientRampValue4 * falloff4 ) ) ) );

        constexpr float kBounding = 0.036730043590068817138671875f;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<SuperSimplex, SIMD> final : public virtual SuperSimplex, public DispatchClass<VariableRange<Seeded<ScalableGenerator>>, SIMD>
{
    FASTNOISE_IMPL_GEN_FN( Gen );

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
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

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z );

        constexpr double kSkew3 = 1.0 / 3.0;
        constexpr double kReflectUnskew3 = -1.0 / 2.0;
        constexpr double kTwiceUnskew3 = -1.0 / 4.0;

        constexpr double kDistanceSquaredA = 3.0 / 4.0;
        constexpr double kDistanceSquaredB = 1.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        float32v skewDelta = float32v( kSkew3 ) * SkewSum( x, y, z );

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

        // Vertex <0, 0, 0>
        float32v value, falloffBaseStemA, falloffBaseStemB;
        {
            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dxBase, dyBase, dzBase );
            float32v falloffBase = FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            value = ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ) * gradientRampValue;
        }

        // Vertex <1, 1, 1> or <-1, -1, -1>
        {
            mask32v signMask = xyzNormal < float32v( 0 );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset = float32v( 3 * kReflectUnskew3 + 1 ) ^ sign;

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimed, yPrimed, zPrimed ), dxBase - offset, dyBase - offset, dzBase - offset );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 1, 0> or <-1, -1, 0>
        {
            mask32v signMask = xyzNormal < zNormal;

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase ), dxBase, dyBase, dzBase - offset0 );
            float32v falloffBase = FS::Min( ( sign ^ dzBase ) - falloffBaseStemB, float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 0, 1> or <-1, 0, -1>
        {
            mask32v signMask = xyzNormal < yNormal;

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed ), dxBase, dyBase - offset0, dzBase );
            float32v falloffBase = FS::Min( ( sign ^ dyBase ) - falloffBaseStemB, float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 1> or <0, -1, -1>
        {
            mask32v signMask = xyzNormal < xNormal;

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( 2 * kReflectUnskew3 ) ^ sign;

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed ), dxBase - offset0, dyBase, dzBase );
            float32v falloffBase = FS::Min( ( sign ^ dxBase ) - falloffBaseStemB, float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 0, 0> or <-1, 0, 0>
        {
            mask32v signMask = xNormal < float32v( 0 );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase ), dxBase + offset0, dyBase - offset0, dzBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 0> or <0, -1, 0>
        {
            mask32v signMask = yNormal < float32v( 0 );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase ), dxBase - offset0, dyBase + offset0, dzBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 1> or <0, 0, -1>
        {
            mask32v signMask = zNormal < float32v( 0 );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );
            float32v offset0 = float32v( kReflectUnskew3 ) ^ sign; // offset1 = -offset0 because kReflectUnskew3 + 1 = -kReflectUnskew3

            float32v gradientRampValue = GetGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed ), dxBase - offset0, dyBase - offset0, dzBase + offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        constexpr float kBounding = 0.0069091119803488254547119140625f;
        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }

    FS_FORCEINLINE float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        seed += int32v( mSeedOffset );
        this->ScalePositions( this, x, y, z, w );

        constexpr double kRoot5 = 2.2360679774997896964091736687313;
        constexpr double kSkew4 = 1.0 / ( kRoot5 + 1.0 );
        constexpr double kUnskew4 = -1.0 / ( kRoot5 + 5.0 );
        constexpr double kTwiceUnskew4 = -1.0 / 5.0;

        constexpr double kDistanceSquaredA = 4.0 / 5.0;
        constexpr double kDistanceSquaredB = 6.0 / 5.0;
        constexpr double kFalloffRadiusSquared = kDistanceSquaredA;

        // MSVC 19.5x miscompiles this sum of scaled positions like the 3D one (see SkewSum), summing the last terms first avoids it
#if defined( _MSC_VER ) && !defined( __clang__ ) && _MSC_VER >= 1950
        float32v skewDelta = float32v( kSkew4 ) * ( x + ( y + ( z + w ) ) );
#else
        float32v skewDelta = float32v( kSkew4 ) * ( x + y + z + w );
#endif

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

        // Vertex <0, 0, 0, 0>
        float32v value, falloffBaseStemA, falloffBaseStemB;
        {
            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase, dyBase, dzBase, dwBase );
            float32v falloffBase = FS::FNMulAdd( dwBase, dwBase, FS::FNMulAdd( dzBase, dzBase, FS::FNMulAdd( dyBase, dyBase, FS::FNMulAdd( dxBase, dxBase, float32v( kFalloffRadiusSquared ) ) ) ) ) * float32v( 0.5f );
            falloffBaseStemA = falloffBase - float32v( kDistanceSquaredA * 0.5 );
            falloffBaseStemB = falloffBase - float32v( kDistanceSquaredB * 0.5 );
            value = ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ) * gradientRampValue;
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

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimed, zPrimed, wPrimed ), dxBase - offset, dyBase - offset, dzBase - offset, dwBase - offset );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset, coordinateSum, falloffBaseStemA ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
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

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimed, zPrimed, wPrimedBase ), dxBase - offset1, dyBase - offset1, dzBase - offset1, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dwBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
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

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase, wPrimed ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dzBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
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

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed, wPrimed ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dyBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
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

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed, wPrimed ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset1, coordinateSum, falloffBaseStemB ) - ( sign ^ dxBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 0, 0, 0> or <-1, 0, 0, 0>
        {
            mask32v signMask = xNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase, wPrimedBase ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dxBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 1, 0, 0> or <-1, -1, 0, 0>
        {
            mask32v signMask = xNormal < -yNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimed, zPrimedBase, wPrimedBase ), dxBase - offset1, dyBase - offset1, dzBase - offset0, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dyBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 0, 1, 0> or <-1, 0, -1, 0>
        {
            mask32v signMask = xNormal < -zNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimedBase, zPrimed, wPrimedBase ), dxBase - offset1, dyBase - offset0, dzBase - offset1, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dzBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <1, 0, 0, 1> or <-1, 0, 0, -1>
        {
            mask32v signMask = xNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v xPrimed = xPrimedBase + FS::Select( signMask, int32v( -Primes::X ), int32v( Primes::X ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimed, yPrimedBase, zPrimedBase, wPrimed ), dxBase - offset1, dyBase - offset0, dzBase - offset0, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dxBase + dwBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 0, 0> or <0, -1, 0, 0>
        {
            mask32v signMask = yNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase, wPrimedBase ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dyBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 1, 0> or <0, -1, -1, 0>
        {
            mask32v signMask = yNormal < -zNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimed, zPrimed, wPrimedBase ), dxBase - offset0, dyBase - offset1, dzBase - offset1, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dzBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 1, 0, 1> or <0, -1, 0, -1>
        {
            mask32v signMask = yNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v yPrimed = yPrimedBase + FS::Select( signMask, int32v( -Primes::Y ), int32v( Primes::Y ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimed, zPrimedBase, wPrimed ), dxBase - offset0, dyBase - offset1, dzBase - offset0, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dyBase + dwBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 1, 0> or <0, 0, -1, 0>
        {
            mask32v signMask = zNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed, wPrimedBase ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset0 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dzBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 1, 1> or <0, 0, -1, -1>
        {
            mask32v signMask = zNormal < -wNormal;
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v zPrimed = zPrimedBase + FS::Select( signMask, int32v( -Primes::Z ), int32v( Primes::Z ) );
            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( 2 * kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( 2 * kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimed, wPrimed ), dxBase - offset0, dyBase - offset0, dzBase - offset1, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemB ) + ( sign ^ ( dzBase + dwBase ) ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        // Vertex <0, 0, 0, 1> or <0, 0, 0, -1>
        {
            mask32v signMask = wNormal < float32v( 0 );
            float32v sign = FS::Masked( signMask, float32v( FS::Cast<float>( int32v( 1 << 31 ) ) ) );

            int32v wPrimed = wPrimedBase + FS::Select( signMask, int32v( -Primes::W ), int32v( Primes::W ) );

            float32v offset1 = float32v( kUnskew4 + 1 ) ^ sign;
            float32v offset0 = float32v( kUnskew4 ) ^ sign;

            float32v gradientRampValue = GetGradientDotPerlin( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimed ), dxBase - offset0, dyBase - offset0, dzBase - offset0, dwBase - offset1 );
            float32v falloffBase = FS::Max( FS::FMulAdd( offset0, coordinateSum, falloffBaseStemA ) + ( sign ^ dwBase ), float32v( 0.0f ) );
            value = FS::FMulAdd( ( falloffBase * falloffBase ) * ( falloffBase * falloffBase ), gradientRampValue, value );
        }

        constexpr float kBounding = 0.011310013942420482635498046875;

        return this->ScaleOutput( this, value, -kBounding, kBounding );
    }
};
