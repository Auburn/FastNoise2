#include "DomainWarpSimplex.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainWarpSimplex, SIMD> final : public virtual FastNoise::DomainWarpSimplex, public FastSIMD::DispatchClass<FastNoise::DomainWarp, SIMD>
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            49.918426513671875 / 2.0 :
            70.1480577066486 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            32.69428253173828125 / 1.4142135623730951 :
            16.281631889139874 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
        valueZ *= float32v( kBounding );
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            33.653125584827855 / 1.4142135623730951 :
            30.88161777516092 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
        valueZ *= float32v( kBounding );
        valueW *= float32v( kBounding );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        return FS::FMulAdd( valueW, valueW, FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::DomainWarpSuperSimplex, SIMD> final : public virtual FastNoise::DomainWarpSuperSimplex, public FastSIMD::DispatchClass<FastNoise::DomainWarp, SIMD>
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            9.28993664146183 / 2.0 :
            12.814453124999995 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            144.736422163332608 / 1.4142135623730951 :
            37.63698669623629 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
        valueZ *= float32v( kBounding );
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

        constexpr double kBounding = ( Scheme == VectorizationScheme::GradientOuterProduct ?
            115.21625311930542 / 1.4142135623730951 :
            48.80058117543753 );

        valueX *= float32v( kBounding );
        valueY *= float32v( kBounding );
        valueZ *= float32v( kBounding );
        valueW *= float32v( kBounding );
        xOut = FS::FMulAdd( valueX, warpAmp, xOut );
        yOut = FS::FMulAdd( valueY, warpAmp, yOut );
        zOut = FS::FMulAdd( valueZ, warpAmp, zOut );

        return FS::FMulAdd( valueW, valueW, FS::FMulAdd( valueZ, valueZ, FS::FMulAdd( valueY, valueY, valueX * valueX ) ) );
    }
};
