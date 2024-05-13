#include "DomainWarpSimplex.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainWarpSimplex, SIMD> final : public virtual FastNoise::DomainWarpSimplex, public FastSIMD::DispatchClass<FastNoise::DomainWarp, SIMD>
{
public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const
    {
        const float SQRT3 = 1.7320508075688772935274463415059f;
        const float F2 = 0.5f * ( SQRT3 - 1.0f );
        const float G2 = ( 3.0f - SQRT3 ) / 6.0f;

        float32v skewDelta = float32v( F2 ) * ( x + y );
        float32v xSkewed = x + skewDelta;
        float32v ySkewed = y + skewDelta;

        float32v xSkewedBase = FS::Floor( xSkewed );
        float32v ySkewedBase = FS::Floor( ySkewed );
        float32v dxSkewed = xSkewed - xSkewedBase;
        float32v dySkewed = ySkewed - ySkewedBase;

        int32v xPrimedBase = FS::Convert<int32_t>( xSkewedBase ) * int32v( Primes::X );
        int32v yPrimedBase = FS::Convert<int32_t>( ySkewedBase ) * int32v( Primes::Y );

        mask32v xGreaterEqualY = dxSkewed >= dySkewed;

        float32v unskewDelta = float32v( G2 ) * ( dxSkewed + dySkewed );
        float32v dx0 = dxSkewed - unskewDelta;
        float32v dy0 = dySkewed - unskewDelta;

        float32v dx1 = FS::MaskedIncrement( ~xGreaterEqualY, dx0 ) + float32v( G2 - 1 );
        float32v dy1 = FS::MaskedIncrement( xGreaterEqualY, dy0 ) + float32v( G2 - 1 );
        float32v dx2 = dx0 + float32v( G2 * 2 - 1 );
        float32v dy2 = dy0 + float32v( G2 * 2 - 1 );

        float32v falloff0 = FS::FNMulAdd( dx0, dx0, FS::FNMulAdd( dy0, dy0, float32v( 0.5f ) ) );
        float32v falloff1 = FS::FNMulAdd( dx1, dx1, FS::FNMulAdd( dy1, dy1, float32v( 0.5f ) ) );
        float32v falloff2 = falloff0 + FS::FMulAdd( unskewDelta, float32v( ( 2 * G2 - 1 ) * ( 2 - 1 / G2 ) * 2 ), float32v( ( 2 * G2 - 1 ) * ( 2 * G2 - 1 ) * -2 ) );

        falloff0 = FS::Max( falloff0, float32v( 0 ) );
        falloff1 = FS::Max( falloff1, float32v( 0 ) );
        falloff2 = FS::Max( falloff2, float32v( 0 ) );

        falloff0 *= falloff0; falloff0 *= falloff0;
        falloff1 *= falloff1; falloff1 *= falloff1;
        falloff2 *= falloff2; falloff2 *= falloff2;

        float32v valueX( 0 );
        float32v valueY( 0 );

        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed, xPrimedBase, yPrimedBase ), dx0, dy0, falloff0, valueX, valueY );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed, FS::MaskedAdd( xGreaterEqualY, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( xGreaterEqualY, yPrimedBase, int32v( Primes::Y ) ) ), dx1, dy1, falloff1, valueX, valueY );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ) ), dx2, dy2, falloff2, valueX, valueY );

        warpAmp *= float32v( 49.918426513671875f * 0.5f );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );

        float32v warpLengthSq = FS::FMulAdd( valueY, valueY, valueX * valueX );
        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const
    {
        const float F3 = 1.0f / 3.0f;
        const float G3 = 1.0f / 2.0f;

        float32v skewDelta = float32v( F3 ) * ( x + y + z );
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

        float32v unskewDelta = float32v( G3 ) * ( dxSkewed + dySkewed + dzSkewed );
        float32v dx0 = dxSkewed - unskewDelta;
        float32v dy0 = dySkewed - unskewDelta;
        float32v dz0 = dzSkewed - unskewDelta;

        mask32v maskX1 = xGreaterEqualY & xGreaterEqualZ;
        mask32v maskY1 = FS::BitwiseAndNot( yGreaterEqualZ, xGreaterEqualY );
        mask32v maskZ1 = FS::BitwiseAndNot( ~xGreaterEqualZ, yGreaterEqualZ );

        mask32v nMaskX2 = ~( xGreaterEqualY | xGreaterEqualZ );
        mask32v nMaskY2 = xGreaterEqualY & ~yGreaterEqualZ;
        mask32v nMaskZ2 = xGreaterEqualZ & yGreaterEqualZ;

        float32v dx3 = dx0 + float32v( G3 * 3 - 1 );
        float32v dy3 = dy0 + float32v( G3 * 3 - 1 );
        float32v dz3 = dz0 + float32v( G3 * 3 - 1 );
        float32v dx1 = FS::MaskedSub( maskX1, dx3, float32v( 1 ) ); // G3 * 3 - 1 = G3, so dx0 + G3 = dx3
        float32v dy1 = FS::MaskedSub( maskY1, dy3, float32v( 1 ) );
        float32v dz1 = FS::MaskedSub( maskZ1, dz3, float32v( 1 ) );
        float32v dx2 = FS::MaskedIncrement( nMaskX2, dx0 ); // G3 * 2 - 1 = 0, so dx0 + ( G3 * 2 - 1 ) = dx0
        float32v dy2 = FS::MaskedIncrement( nMaskY2, dy0 );
        float32v dz2 = FS::MaskedIncrement( nMaskZ2, dz0 );

        float32v falloff0 = FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( 0.6f ) ) ) );
        float32v falloff1 = FS::FNMulAdd( dz1, dz1, FS::FNMulAdd( dy1, dy1, FS::FNMulAdd( dx1, dx1, float32v( 0.6f ) ) ) );
        float32v falloff2 = FS::FNMulAdd( dz2, dz2, FS::FNMulAdd( dy2, dy2, FS::FNMulAdd( dx2, dx2, float32v( 0.6f ) ) ) );
        float32v falloff3 = falloff0 + FS::FMulAdd( unskewDelta, float32v( ( 3 * G3 - 1 ) * ( 3 - 1 / G3 ) * 2 ), float32v( ( 3 * G3 - 1 ) * ( 3 * G3 - 1 ) * -3 ) );

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

        ApplyVectorMaskedGradientDotCommon( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase ), dx0, dy0, dz0, falloff0, valueX, valueY, valueZ );
        ApplyVectorMaskedGradientDotCommon( HashPrimes( seed, FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ), FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ), FS::MaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ) ), dx1, dy1, dz1, falloff1, valueX, valueY, valueZ );
        ApplyVectorMaskedGradientDotCommon( HashPrimes( seed, FS::InvMaskedAdd( nMaskX2, xPrimedBase, int32v( Primes::X ) ), FS::InvMaskedAdd( nMaskY2, yPrimedBase, int32v( Primes::Y ) ), FS::InvMaskedAdd( nMaskZ2, zPrimedBase, int32v( Primes::Z ) ) ), dx2, dy2, dz2, falloff2, valueX, valueY, valueZ );
        ApplyVectorMaskedGradientDotCommon( HashPrimes( seed, xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ) ), dx3, dy3, dz3, falloff3, valueX, valueY, valueZ );

        // Match gradient orientation.
        float32v valueTransformDelta = float32v( -2.0f / 3.0f ) * ( valueX + valueY + valueZ );
        valueX += valueTransformDelta;
        valueY += valueTransformDelta;
        valueZ += valueTransformDelta;

        warpAmp *= float32v( 32.69428253173828125f * 0.7071067811865475f );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        float32v warpLengthSq = FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) );
        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const
    {
        const float SQRT5 = 2.236067977499f;
        const float F4 = ( SQRT5 - 1.0f ) / 4.0f;
        const float G4 = ( 5.0f - SQRT5 ) / 20.0f;

        float32v skewDelta = float32v( F4 ) * ( x + y + z + w );
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

        float32v unskewDelta = float32v( G4 ) * ( dxSkewed + dySkewed + dzSkewed + dwSkewed );
        float32v dx0 = dxSkewed - unskewDelta;
        float32v dy0 = dySkewed - unskewDelta;
        float32v dz0 = dzSkewed - unskewDelta;
        float32v dw0 = dwSkewed - unskewDelta;

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

        float32v dx1 = FS::MaskedSub( maskX1, dx0, float32v( 1 ) ) + float32v( G4 );
        float32v dy1 = FS::MaskedSub( maskY1, dy0, float32v( 1 ) ) + float32v( G4 );
        float32v dz1 = FS::MaskedSub( maskZ1, dz0, float32v( 1 ) ) + float32v( G4 );
        float32v dw1 = FS::MaskedSub( maskW1, dw0, float32v( 1 ) ) + float32v( G4 );
        float32v dx2 = FS::MaskedSub( maskX2, dx0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v dy2 = FS::MaskedSub( maskY2, dy0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v dz2 = FS::MaskedSub( maskZ2, dz0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v dw2 = FS::MaskedSub( maskW2, dw0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v dx3 = FS::MaskedSub( maskX3, dx0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v dy3 = FS::MaskedSub( maskY3, dy0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v dz3 = FS::MaskedSub( maskZ3, dz0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v dw3 = FS::MaskedSub( maskW3, dw0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v dx4 = dx0 + float32v( G4 * 4 - 1 );
        float32v dy4 = dy0 + float32v( G4 * 4 - 1 );
        float32v dz4 = dz0 + float32v( G4 * 4 - 1 );
        float32v dw4 = dw0 + float32v( G4 * 4 - 1 );

        float32v falloff0 = FS::FNMulAdd( dw0, dw0, FS::FNMulAdd( dz0, dz0, FS::FNMulAdd( dy0, dy0, FS::FNMulAdd( dx0, dx0, float32v( 0.6f ) ) ) ) );
        float32v falloff1 = FS::FNMulAdd( dw1, dw1, FS::FNMulAdd( dz1, dz1, FS::FNMulAdd( dy1, dy1, FS::FNMulAdd( dx1, dx1, float32v( 0.6f ) ) ) ) );
        float32v falloff2 = FS::FNMulAdd( dw2, dw2, FS::FNMulAdd( dz2, dz2, FS::FNMulAdd( dy2, dy2, FS::FNMulAdd( dx2, dx2, float32v( 0.6f ) ) ) ) );
        float32v falloff3 = FS::FNMulAdd( dw3, dw3, FS::FNMulAdd( dz3, dz3, FS::FNMulAdd( dy3, dy3, FS::FNMulAdd( dx3, dx3, float32v( 0.6f ) ) ) ) );
        float32v falloff4 = falloff0 + FS::FMulAdd( unskewDelta, float32v( ( 4 * G4 - 1 ) * ( 4 - 1 / G4 ) * 2 ), float32v( ( 4 * G4 - 1 ) * ( 4 * G4 - 1 ) * -4 ) );

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

        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed, xPrimedBase, yPrimedBase, zPrimedBase, wPrimedBase ), dx0, dy0, dz0, dw0, falloff0, valueX, valueY, valueZ, valueW );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed,
            FS::MaskedAdd( maskX1, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY1, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ1, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW1, wPrimedBase, int32v( Primes::W ) ) ), dx1, dy1, dz1, dw1, falloff1, valueX, valueY, valueZ, valueW );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed,
            FS::MaskedAdd( maskX2, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY2, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ2, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW2, wPrimedBase, int32v( Primes::W ) ) ), dx2, dy2, dz2, dw2, falloff2, valueX, valueY, valueZ, valueW );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed,
            FS::MaskedAdd( maskX3, xPrimedBase, int32v( Primes::X ) ),
            FS::MaskedAdd( maskY3, yPrimedBase, int32v( Primes::Y ) ),
            FS::MaskedAdd( maskZ3, zPrimedBase, int32v( Primes::Z ) ),
            FS::MaskedAdd( maskW3, wPrimedBase, int32v( Primes::W ) ) ), dx3, dy3, dz3, dw3, falloff3, valueX, valueY, valueZ, valueW );
        ApplyVectorMaskedGradientDotSimplex( HashPrimes( seed,
            xPrimedBase + int32v( Primes::X ), yPrimedBase + int32v( Primes::Y ), zPrimedBase + int32v( Primes::Z ), wPrimedBase + int32v( Primes::W ) ),
            dx4, dy4, dz4, dw4, falloff4, valueX, valueY, valueZ, valueW );

        warpAmp *= float32v( 33.653125584827855f * 0.7071067811865475f );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        constexpr float kBounding = 33.653125584827855f;

        float32v warpLengthSq = FS::FMulAdd( valueW, valueW, FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) ) );
        return warpLengthSq * FS::InvSqrt( warpLengthSq );
    }
};
