#include "Simplex.h"
#include "Utils.inl"

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::Simplex, SIMD> final : public virtual FastNoise::Simplex, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        this->ScalePositions( x, y );

        const float SQRT3 = 1.7320508075688772935274463415059f;
        const float F2 = 0.5f * (SQRT3 - 1.0f);
        const float G2 = (3.0f - SQRT3) / 6.0f;

        float32v f = float32v( F2 ) * (x + y);
        float32v x0 = FS::Floor( x + f );
        float32v y0 = FS::Floor( y + f );

        int32v i = FS::Convert<int32_t>( x0 ) * int32v( Primes::X );
        int32v j = FS::Convert<int32_t>( y0 ) * int32v( Primes::Y );

        float32v g = float32v( G2 ) * (x0 + y0);
        x0 = x - (x0 - g);
        y0 = y - (y0 - g);

        mask32v i1 = x0 > y0;
        //mask32v j1 = ~i1; //InvMasked funcs

        float32v x1 = FS::MaskedSub( i1, x0, float32v( 1.f ) ) + float32v( G2 );
        float32v y1 = FS::InvMaskedSub( i1, y0, float32v( 1.f ) ) + float32v( G2 );

        float32v x2 = x0 + float32v( G2 * 2 - 1 );
        float32v y2 = y0 + float32v( G2 * 2 - 1 );

        float32v t0 = FS::FNMulAdd( x0, x0, FS::FNMulAdd( y0, y0, float32v( 0.5f ) ) );
        float32v t1 = FS::FNMulAdd( x1, x1, FS::FNMulAdd( y1, y1, float32v( 0.5f ) ) );
        float32v t2 = FS::FNMulAdd( x2, x2, FS::FNMulAdd( y2, y2, float32v( 0.5f ) ) );

        t0 = FS::Max( t0, float32v( 0 ) );
        t1 = FS::Max( t1, float32v( 0 ) );
        t2 = FS::Max( t2, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;

        float32v n0 = GetGradientDot( HashPrimes( seed, i, j ), x0, y0 );
        float32v n1 = GetGradientDot( HashPrimes( seed, FS::MaskedAdd( i1, i, int32v( Primes::X ) ), FS::InvMaskedAdd( i1, j, int32v( Primes::Y ) ) ), x1, y1 );
        float32v n2 = GetGradientDot( HashPrimes( seed, i + int32v( Primes::X ), j + int32v( Primes::Y ) ), x2, y2 );

        return float32v( 38.283687591552734375f ) * FS::FMulAdd( n0, t0, FS::FMulAdd( n1, t1, n2 * t2 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        this->ScalePositions( x, y, z );

        const float F3 = 1.0f / 3.0f;
        const float G3 = 1.0f / 2.0f;

        float32v s = float32v( F3 ) * (x + y + z);
        x += s;
        y += s;
        z += s;

        float32v x0 = FS::Floor( x );
        float32v y0 = FS::Floor( y );
        float32v z0 = FS::Floor( z );
        float32v xi = x - x0;
        float32v yi = y - y0;
        float32v zi = z - z0;

        int32v i = FS::Convert<int32_t>( x0 ) * int32v( Primes::X );
        int32v j = FS::Convert<int32_t>( y0 ) * int32v( Primes::Y );
        int32v k = FS::Convert<int32_t>( z0 ) * int32v( Primes::Z );

        mask32v x_ge_y = xi >= yi;
        mask32v y_ge_z = yi >= zi;
        mask32v x_ge_z = xi >= zi;

        float32v g = float32v( G3 ) * (xi + yi + zi);
        x0 = xi - g;
        y0 = yi - g;
        z0 = zi - g;

        mask32v i1 = x_ge_y & x_ge_z;
        mask32v j1 = FS::BitwiseAndNot( y_ge_z, x_ge_y );
        mask32v k1 = FS::BitwiseAndNot( ~x_ge_z, y_ge_z );

        mask32v i2 = x_ge_y | x_ge_z;
        mask32v j2 = ~x_ge_y | y_ge_z;
        mask32v k2 = x_ge_z & y_ge_z; //InvMasked

        float32v x1 = FS::MaskedSub( i1, x0, float32v( 1 ) ) + float32v( G3 );
        float32v y1 = FS::MaskedSub( j1, y0, float32v( 1 ) ) + float32v( G3 );
        float32v z1 = FS::MaskedSub( k1, z0, float32v( 1 ) ) + float32v( G3 );
        float32v x2 = FS::MaskedSub( i2, x0, float32v( 1 ) ) + float32v( G3 * 2 );
        float32v y2 = FS::MaskedSub( j2, y0, float32v( 1 ) ) + float32v( G3 * 2 );
        float32v z2 = FS::InvMaskedSub( k2, z0, float32v( 1 ) ) + float32v( G3 * 2 );
        float32v x3 = x0 + float32v( G3 * 3 - 1 );
        float32v y3 = y0 + float32v( G3 * 3 - 1 );
        float32v z3 = z0 + float32v( G3 * 3 - 1 );

        float32v t0 = FS::FNMulAdd( x0, x0, FS::FNMulAdd( y0, y0, FS::FNMulAdd( z0, z0, float32v( 0.6f ) ) ) );
        float32v t1 = FS::FNMulAdd( x1, x1, FS::FNMulAdd( y1, y1, FS::FNMulAdd( z1, z1, float32v( 0.6f ) ) ) );
        float32v t2 = FS::FNMulAdd( x2, x2, FS::FNMulAdd( y2, y2, FS::FNMulAdd( z2, z2, float32v( 0.6f ) ) ) );
        float32v t3 = FS::FNMulAdd( x3, x3, FS::FNMulAdd( y3, y3, FS::FNMulAdd( z3, z3, float32v( 0.6f ) ) ) );

        t0 = FS::Max( t0, float32v( 0 ) );
        t1 = FS::Max( t1, float32v( 0 ) );
        t2 = FS::Max( t2, float32v( 0 ) );
        t3 = FS::Max( t3, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;
        t3 *= t3; t3 *= t3;             

        float32v n0 = GetGradientDot( HashPrimes( seed, i, j, k ), x0, y0, z0 );
        float32v n1 = GetGradientDot( HashPrimes( seed, FS::MaskedAdd( i1, i, int32v( Primes::X ) ), FS::MaskedAdd( j1, j, int32v( Primes::Y ) ), FS::MaskedAdd( k1, k, int32v( Primes::Z ) ) ), x1, y1, z1 );
        float32v n2 = GetGradientDot( HashPrimes( seed, FS::MaskedAdd( i2, i, int32v( Primes::X ) ), FS::MaskedAdd( j2, j, int32v( Primes::Y ) ), FS::InvMaskedAdd( k2, k, int32v( Primes::Z ) ) ), x2, y2, z2 );
        float32v n3 = GetGradientDot( HashPrimes( seed, i + int32v( Primes::X ), j + int32v( Primes::Y ), k + int32v( Primes::Z ) ), x3, y3, z3 );

        return float32v( 32.69428253173828125f ) * FS::FMulAdd( n0, t0, FS::FMulAdd( n1, t1, FS::FMulAdd( n2, t2, n3 * t3 ) ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        this->ScalePositions( x, y, z, w );

        const float SQRT5 = 2.236067977499f;
        const float F4 = (SQRT5 - 1.0f) / 4.0f;
        const float G4 = (5.0f - SQRT5) / 20.0f;

        float32v s = float32v( F4 ) * (x + y + z + w);
        x += s;
        y += s;
        z += s;
        w += s;

        float32v x0 = FS::Floor( x );
        float32v y0 = FS::Floor( y );
        float32v z0 = FS::Floor( z );
        float32v w0 = FS::Floor( w );
        float32v xi = x - x0;
        float32v yi = y - y0;
        float32v zi = z - z0;
        float32v wi = w - w0;

        int32v i = FS::Convert<int32_t>( x0 ) * int32v( Primes::X );
        int32v j = FS::Convert<int32_t>( y0 ) * int32v( Primes::Y );
        int32v k = FS::Convert<int32_t>( z0 ) * int32v( Primes::Z );
        int32v l = FS::Convert<int32_t>( w0 ) * int32v( Primes::W );

        float32v g = float32v( G4 ) * (xi + yi + zi + wi);
        x0 = xi - g;
        y0 = yi - g;
        z0 = zi - g;
        w0 = wi - g;

        int32v rankx( 0 );
        int32v ranky( 0 );
        int32v rankz( 0 );
        int32v rankw( 0 );

        mask32v x_ge_y = x0 >= y0;
        rankx = FS::MaskedIncrement( x_ge_y, rankx );
        ranky = FS::MaskedIncrement( ~x_ge_y, ranky );

        mask32v x_ge_z = x0 >= z0;
        rankx = FS::MaskedIncrement( x_ge_z, rankx );
        rankz = FS::MaskedIncrement( ~x_ge_z, rankz );

        mask32v x_ge_w = x0 >= w0;
        rankx = FS::MaskedIncrement( x_ge_w, rankx );
        rankw = FS::MaskedIncrement( ~x_ge_w, rankw );

        mask32v y_ge_z = y0 >= z0;
        ranky = FS::MaskedIncrement( y_ge_z, ranky );
        rankz = FS::MaskedIncrement( ~y_ge_z, rankz );

        mask32v y_ge_w = y0 >= w0;
        ranky = FS::MaskedIncrement( y_ge_w, ranky );
        rankw = FS::MaskedIncrement( ~y_ge_w, rankw );

        mask32v z_ge_w = z0 >= w0;
        rankz = FS::MaskedIncrement( z_ge_w, rankz );
        rankw = FS::MaskedIncrement( ~z_ge_w, rankw );

        mask32v i1 = rankx > int32v( 2 );
        mask32v j1 = ranky > int32v( 2 );
        mask32v k1 = rankz > int32v( 2 );
        mask32v l1 = rankw > int32v( 2 );

        mask32v i2 = rankx > int32v( 1 );
        mask32v j2 = ranky > int32v( 1 );
        mask32v k2 = rankz > int32v( 1 );
        mask32v l2 = rankw > int32v( 1 );

        mask32v i3 = rankx > int32v( 0 );
        mask32v j3 = ranky > int32v( 0 );
        mask32v k3 = rankz > int32v( 0 );
        mask32v l3 = rankw > int32v( 0 );

        float32v x1 = FS::MaskedSub( i1, x0, float32v( 1 ) ) + float32v( G4 );
        float32v y1 = FS::MaskedSub( j1, y0, float32v( 1 ) ) + float32v( G4 );
        float32v z1 = FS::MaskedSub( k1, z0, float32v( 1 ) ) + float32v( G4 );
        float32v w1 = FS::MaskedSub( l1, w0, float32v( 1 ) ) + float32v( G4 );
        float32v x2 = FS::MaskedSub( i2, x0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v y2 = FS::MaskedSub( j2, y0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v z2 = FS::MaskedSub( k2, z0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v w2 = FS::MaskedSub( l2, w0, float32v( 1 ) ) + float32v( G4 * 2 );
        float32v x3 = FS::MaskedSub( i3, x0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v y3 = FS::MaskedSub( j3, y0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v z3 = FS::MaskedSub( k3, z0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v w3 = FS::MaskedSub( l3, w0, float32v( 1 ) ) + float32v( G4 * 3 );
        float32v x4 = x0 + float32v( G4 * 4 - 1 );
        float32v y4 = y0 + float32v( G4 * 4 - 1 );
        float32v z4 = z0 + float32v( G4 * 4 - 1 );
        float32v w4 = w0 + float32v( G4 * 4 - 1 );

        float32v t0 = FS::FNMulAdd( x0, x0, FS::FNMulAdd( y0, y0, FS::FNMulAdd( z0, z0, FS::FNMulAdd( w0, w0, float32v( 0.6f ) ) ) ) );
        float32v t1 = FS::FNMulAdd( x1, x1, FS::FNMulAdd( y1, y1, FS::FNMulAdd( z1, z1, FS::FNMulAdd( w1, w1, float32v( 0.6f ) ) ) ) );
        float32v t2 = FS::FNMulAdd( x2, x2, FS::FNMulAdd( y2, y2, FS::FNMulAdd( z2, z2, FS::FNMulAdd( w2, w2, float32v( 0.6f ) ) ) ) );
        float32v t3 = FS::FNMulAdd( x3, x3, FS::FNMulAdd( y3, y3, FS::FNMulAdd( z3, z3, FS::FNMulAdd( w3, w3, float32v( 0.6f ) ) ) ) );
        float32v t4 = FS::FNMulAdd( x4, x4, FS::FNMulAdd( y4, y4, FS::FNMulAdd( z4, z4, FS::FNMulAdd( w4, w4, float32v( 0.6f ) ) ) ) );

        t0 = FS::Max( t0, float32v( 0 ) );
        t1 = FS::Max( t1, float32v( 0 ) );
        t2 = FS::Max( t2, float32v( 0 ) );
        t3 = FS::Max( t3, float32v( 0 ) );
        t4 = FS::Max( t4, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;
        t3 *= t3; t3 *= t3;
        t4 *= t4; t4 *= t4;

        float32v n0 = GetGradientDot( HashPrimes( seed, i, j, k, l ), x0, y0, z0, w0 );
        float32v n1 = GetGradientDot( HashPrimes( seed, 
            FS::MaskedAdd( i1, i, int32v( Primes::X ) ),
            FS::MaskedAdd( j1, j, int32v( Primes::Y ) ),
            FS::MaskedAdd( k1, k, int32v( Primes::Z ) ),
            FS::MaskedAdd( l1, l, int32v( Primes::W ) ) ), x1, y1, z1, w1 );
        float32v n2 = GetGradientDot( HashPrimes( seed, 
            FS::MaskedAdd( i2, i, int32v( Primes::X ) ),
            FS::MaskedAdd( j2, j, int32v( Primes::Y ) ),
            FS::MaskedAdd( k2, k, int32v( Primes::Z ) ),
            FS::MaskedAdd( l2, l, int32v( Primes::W ) ) ), x2, y2, z2, w2 );
        float32v n3 = GetGradientDot( HashPrimes( seed,
            FS::MaskedAdd( i3, i, int32v( Primes::X ) ),
            FS::MaskedAdd( j3, j, int32v( Primes::Y ) ),
            FS::MaskedAdd( k3, k, int32v( Primes::Z ) ),
            FS::MaskedAdd( l3, l, int32v( Primes::W ) ) ), x3, y3, z3, w3 );
        float32v n4 = GetGradientDot( HashPrimes( seed, i + int32v( Primes::X ), j + int32v( Primes::Y ), k + int32v( Primes::Z ), l + int32v( Primes::W ) ), x4, y4, z4, w4 );

        return float32v( 27.f ) * FS::FMulAdd( n0, t0, FS::FMulAdd( n1, t1, FS::FMulAdd( n2, t2, FS::FMulAdd( n3, t3, n4 * t4 ) ) ) );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::OpenSimplex2, SIMD> final : public virtual FastNoise::OpenSimplex2, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        this->ScalePositions( x, y );

        const float SQRT3 = 1.7320508075f;
        const float F2 = 0.5f * (SQRT3 - 1.0f);
        const float G2 = (3.0f - SQRT3) / 6.0f;

        float32v f = float32v( F2 ) * (x + y);
        float32v x0 = FS::Floor( x + f );
        float32v y0 = FS::Floor( y + f );

        int32v i = FS::Convert<int32_t>( x0 ) * int32v( Primes::X );
        int32v j = FS::Convert<int32_t>( y0 ) * int32v( Primes::Y );

        float32v g = float32v( G2 ) * (x0 + y0);
        x0 = x - (x0 - g);
        y0 = y - (y0 - g);

        mask32v i1 = x0 > y0;
        //mask32v j1 = ~i1; //InvMasked funcs

        float32v x1 = FS::MaskedSub( i1, x0, float32v( 1.f ) ) + float32v( G2 );
        float32v y1 = FS::InvMaskedSub( i1, y0, float32v( 1.f ) ) + float32v( G2 );
        float32v x2 = x0 + float32v( (G2 * 2) - 1 );
        float32v y2 = y0 + float32v( (G2 * 2) - 1 );

        float32v t0 = float32v( 0.5f ) - (x0 * x0) - (y0 * y0);
        float32v t1 = float32v( 0.5f ) - (x1 * x1) - (y1 * y1);
        float32v t2 = float32v( 0.5f ) - (x2 * x2) - (y2 * y2);

        t0 = FS::Max( t0, float32v( 0 ) );
        t1 = FS::Max( t1, float32v( 0 ) );
        t2 = FS::Max( t2, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;

        float32v n0 = GetGradientDotFancy( HashPrimes( seed, i, j ), x0, y0 );
        float32v n1 = GetGradientDotFancy( HashPrimes( seed, FS::MaskedAdd( i1, i, int32v( Primes::X ) ), FS::InvMaskedAdd( i1, j, int32v( Primes::Y ) ) ), x1, y1 );
        float32v n2 = GetGradientDotFancy( HashPrimes( seed, i + int32v( Primes::X ), j + int32v( Primes::Y ) ), x2, y2 );

        return float32v( 49.918426513671875f ) * FS::FMulAdd( n0, t0, FS::FMulAdd( n1, t1, n2 * t2 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        this->ScalePositions( x, y, z );

        float32v f = float32v( 2.0f / 3.0f ) * (x + y + z);
        float32v xr = f - x;
        float32v yr = f - y;
        float32v zr = f - z;

        float32v val( 0 );
        for( size_t i = 0; ; i++ )
        {
            float32v v0xr = FS::Round( xr );
            float32v v0yr = FS::Round( yr );
            float32v v0zr = FS::Round( zr );
            float32v d0xr = xr - v0xr;
            float32v d0yr = yr - v0yr;
            float32v d0zr = zr - v0zr;

            float32v score0xr = FS::Abs( d0xr );
            float32v score0yr = FS::Abs( d0yr );
            float32v score0zr = FS::Abs( d0zr );
            mask32v dir0xr = FS::Max( score0yr, score0zr ) <= score0xr;
            mask32v dir0yr = FS::BitwiseAndNot( FS::Max( score0zr, score0xr ) <= score0yr, dir0xr );
            mask32v dir0zr = ~(dir0xr | dir0yr);
            float32v v1xr = FS::MaskedAdd( dir0xr, v0xr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0xr ) );
            float32v v1yr = FS::MaskedAdd( dir0yr, v0yr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0yr ) );
            float32v v1zr = FS::MaskedAdd( dir0zr, v0zr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0zr ) );
            float32v d1xr = xr - v1xr;
            float32v d1yr = yr - v1yr;
            float32v d1zr = zr - v1zr;

            int32v hv0xr = FS::Convert<int32_t>( v0xr ) * int32v( Primes::X );
            int32v hv0yr = FS::Convert<int32_t>( v0yr ) * int32v( Primes::Y );
            int32v hv0zr = FS::Convert<int32_t>( v0zr ) * int32v( Primes::Z );

            int32v hv1xr = FS::Convert<int32_t>( v1xr ) * int32v( Primes::X );
            int32v hv1yr = FS::Convert<int32_t>( v1yr ) * int32v( Primes::Y );
            int32v hv1zr = FS::Convert<int32_t>( v1zr ) * int32v( Primes::Z );

            float32v t0 = FS::FNMulAdd( d0zr, d0zr, FS::FNMulAdd( d0yr, d0yr, FS::FNMulAdd( d0xr, d0xr, float32v( 0.6f ) ) ) );
            float32v t1 = FS::FNMulAdd( d1zr, d1zr, FS::FNMulAdd( d1yr, d1yr, FS::FNMulAdd( d1xr, d1xr, float32v( 0.6f ) ) ) );
            t0 = FS::Max( t0, float32v( 0 ) );
            t1 = FS::Max( t1, float32v( 0 ) );
            t0 *= t0; t0 *= t0;
            t1 *= t1; t1 *= t1;

            float32v v0 = GetGradientDot( HashPrimes( seed, hv0xr, hv0yr, hv0zr ), d0xr, d0yr, d0zr );
            float32v v1 = GetGradientDot( HashPrimes( seed, hv1xr, hv1yr, hv1zr ), d1xr, d1yr, d1zr );

            val = FS::FMulAdd( v0, t0, FS::FMulAdd( v1, t1, val ) );

            if( i == 1 )
            {
                break;
            }

            xr += float32v( 0.5f );
            yr += float32v( 0.5f );
            zr += float32v( 0.5f );
            seed = ~seed;
        }

        return float32v( 32.69428253173828125f ) * val;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        this->ScalePositions( x, y, z, w );

        float32v f = float32v( -0.138196601125011f ) * ( x + y + z + w );
        float32v xs = x + f;
        float32v ys = y + f;
        float32v zs = z + f;
        float32v ws = w + f;

        float32v xsi = FS::Floor( xs );
        float32v ysi = FS::Floor( ys );
        float32v zsi = FS::Floor( zs );
        float32v wsi = FS::Floor( ws );
        int32v xsb = FS::Convert<int>( xsi ) * int32v( Primes::X );
        int32v ysb = FS::Convert<int>( ysi ) * int32v( Primes::Y );
        int32v zsb = FS::Convert<int>( zsi ) * int32v( Primes::Z );
        int32v wsb = FS::Convert<int>( wsi ) * int32v( Primes::W );
        xsi = xs - xsi;
        ysi = ys - ysi;
        zsi = zs - zsi;
        wsi = ws - wsi;

        float32v sumsi = xsi + ysi + zsi + wsi;
        
        float32v firstLattice = FS::Floor( sumsi * float32v( 1.25f ) );

        sumsi -= firstLattice * float32v( 0.8f );

        float32v firstLattice02 = firstLattice * float32v( 0.2f );
        xsi -= firstLattice02;
        ysi -= firstLattice02;
        zsi -= firstLattice02;
        wsi -= firstLattice02;

        int32v firstLatticeI = FS::Convert<int>( firstLattice );
        int32v firstLattice128 = firstLatticeI << 7;
        xsb -= firstLattice128;                
        ysb -= firstLattice128;            
        zsb -= firstLattice128;        
        wsb -= firstLattice128;

        int32v xLatticeCoordOffset = FS::MaskedSub( firstLatticeI == int32v( 0 ), int32v( 128 * Primes::X ), int32v( 641 * Primes::X ) );
        int32v yLatticeCoordOffset = FS::MaskedSub( firstLatticeI == int32v( 1 ), int32v( 128 * Primes::Y ), int32v( 641 * Primes::Y ) );
        int32v zLatticeCoordOffset = FS::MaskedSub( firstLatticeI == int32v( 2 ), int32v( 128 * Primes::Z ), int32v( 641 * Primes::Z ) );
        int32v wLatticeCoordOffset = FS::MaskedSub( firstLatticeI == int32v( 3 ), int32v( 128 * Primes::W ), int32v( 641 * Primes::W ) );

        // 0
        mask32v v0x = xsi >= FS::Max( FS::Max( ysi, zsi ), FS::Max( wsi, float32v( 1 ) - sumsi ) );
        mask32v v0y = ysi >= FS::Max( FS::Max( zsi, wsi ), FS::Max( xsi, float32v( 1 ) - sumsi ) );
        mask32v v0z = zsi >= FS::Max( FS::Max( wsi, xsi ), FS::Max( ysi, float32v( 1 ) - sumsi ) );
        mask32v v0w = wsi >= FS::Max( FS::Max( xsi, ysi ), FS::Max( zsi, float32v( 1 ) - sumsi ) );

        int32v vh0x = FS::MaskedAdd( v0x, xsb, int32v( Primes::X ) );
        int32v vh0y = FS::MaskedAdd( v0y, ysb, int32v( Primes::Y ) );
        int32v vh0z = FS::MaskedAdd( v0z, zsb, int32v( Primes::Z ) );
        int32v vh0w = FS::MaskedAdd( v0w, wsb, int32v( Primes::W ) ); 

        xsb = vh0x + xLatticeCoordOffset;
        ysb = vh0y + xLatticeCoordOffset;
        zsb = vh0z + xLatticeCoordOffset;
        wsb = vh0w + xLatticeCoordOffset;
        xsi = FS::MaskedDecrement( v0x, xsi );
        ysi = FS::MaskedDecrement( v0y, ysi );
        zsi = FS::MaskedDecrement( v0z, zsi );
        wsi = FS::MaskedDecrement( v0w, wsi );
        sumsi = xsi + ysi + zsi + wsi;

        float32v d0x = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), xsi );
        float32v d0y = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), ysi );
        float32v d0z = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), zsi );
        float32v d0w = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), wsi );

        xsi += float32v( 0.2f );
        ysi += float32v( 0.2f );
        zsi += float32v( 0.2f );
        wsi += float32v( 0.2f );
        sumsi += float32v( 0.8f );

        // 1
        mask32v v1x = xsi >= FS::Max( FS::Max( ysi, zsi ), FS::Max( wsi, float32v( 1 ) - sumsi ) );
        mask32v v1y = ysi >= FS::Max( FS::Max( zsi, wsi ), FS::Max( xsi, float32v( 1 ) - sumsi ) );
        mask32v v1z = zsi >= FS::Max( FS::Max( wsi, xsi ), FS::Max( ysi, float32v( 1 ) - sumsi ) );
        mask32v v1w = wsi >= FS::Max( FS::Max( xsi, ysi ), FS::Max( zsi, float32v( 1 ) - sumsi ) );

        int32v vh1x = FS::MaskedAdd( v1x, xsb, int32v( Primes::X ) );
        int32v vh1y = FS::MaskedAdd( v1y, ysb, int32v( Primes::Y ) );
        int32v vh1z = FS::MaskedAdd( v1z, zsb, int32v( Primes::Z ) );
        int32v vh1w = FS::MaskedAdd( v1w, wsb, int32v( Primes::W ) );

        xsb = vh1x + yLatticeCoordOffset;
        ysb = vh1y + yLatticeCoordOffset;
        zsb = vh1z + yLatticeCoordOffset;
        wsb = vh1w + yLatticeCoordOffset;
        xsi = FS::MaskedDecrement( v1x, xsi );
        ysi = FS::MaskedDecrement( v1y, ysi );
        zsi = FS::MaskedDecrement( v1z, zsi );
        wsi = FS::MaskedDecrement( v1w, wsi );
        sumsi = xsi + ysi + zsi + wsi;

        float32v d1x = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), xsi );
        float32v d1y = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), ysi );
        float32v d1z = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), zsi );
        float32v d1w = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), wsi );

        xsi += float32v( 0.2f );
        ysi += float32v( 0.2f );
        zsi += float32v( 0.2f );
        wsi += float32v( 0.2f );
        sumsi += float32v( 0.8f );

        // 2
        mask32v v2x = xsi >= FS::Max( FS::Max( ysi, zsi ), FS::Max( wsi, float32v( 1 ) - sumsi ) );
        mask32v v2y = ysi >= FS::Max( FS::Max( zsi, wsi ), FS::Max( xsi, float32v( 1 ) - sumsi ) );
        mask32v v2z = zsi >= FS::Max( FS::Max( wsi, xsi ), FS::Max( ysi, float32v( 1 ) - sumsi ) );
        mask32v v2w = wsi >= FS::Max( FS::Max( xsi, ysi ), FS::Max( zsi, float32v( 1 ) - sumsi ) );

        int32v vh2x = FS::MaskedAdd( v2x, xsb, int32v( Primes::X ) );
        int32v vh2y = FS::MaskedAdd( v2y, ysb, int32v( Primes::Y ) );
        int32v vh2z = FS::MaskedAdd( v2z, zsb, int32v( Primes::Z ) );
        int32v vh2w = FS::MaskedAdd( v2w, wsb, int32v( Primes::W ) );

        xsb = vh2x + zLatticeCoordOffset;
        ysb = vh2y + zLatticeCoordOffset;
        zsb = vh2z + zLatticeCoordOffset;
        wsb = vh2w + zLatticeCoordOffset;
        xsi = FS::MaskedDecrement( v2x, xsi );
        ysi = FS::MaskedDecrement( v2y, ysi );
        zsi = FS::MaskedDecrement( v2z, zsi );
        wsi = FS::MaskedDecrement( v2w, wsi );
        sumsi = xsi + ysi + zsi + wsi;

        float32v d2x = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), xsi );
        float32v d2y = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), ysi );
        float32v d2z = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), zsi );
        float32v d2w = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), wsi );

        xsi += float32v( 0.2f );
        ysi += float32v( 0.2f );
        zsi += float32v( 0.2f );
        wsi += float32v( 0.2f );
        sumsi += float32v( 0.8f );

        // 3
        mask32v v3x = xsi >= FS::Max( FS::Max( ysi, zsi ), FS::Max( wsi, float32v( 1 ) - sumsi ) );
        mask32v v3y = ysi >= FS::Max( FS::Max( zsi, wsi ), FS::Max( xsi, float32v( 1 ) - sumsi ) );
        mask32v v3z = zsi >= FS::Max( FS::Max( wsi, xsi ), FS::Max( ysi, float32v( 1 ) - sumsi ) );
        mask32v v3w = wsi >= FS::Max( FS::Max( xsi, ysi ), FS::Max( zsi, float32v( 1 ) - sumsi ) );

        int32v vh3x = FS::MaskedAdd( v3x, xsb, int32v( Primes::X ) );
        int32v vh3y = FS::MaskedAdd( v3y, ysb, int32v( Primes::Y ) );
        int32v vh3z = FS::MaskedAdd( v3z, zsb, int32v( Primes::Z ) );
        int32v vh3w = FS::MaskedAdd( v3w, wsb, int32v( Primes::W ) );

        xsb = vh3x + wLatticeCoordOffset;
        ysb = vh3y + wLatticeCoordOffset;
        zsb = vh3z + wLatticeCoordOffset;
        wsb = vh3w + wLatticeCoordOffset;
        xsi = FS::MaskedDecrement( v3x, xsi );
        ysi = FS::MaskedDecrement( v3y, ysi );
        zsi = FS::MaskedDecrement( v3z, zsi );
        wsi = FS::MaskedDecrement( v3w, wsi );
        sumsi = xsi + ysi + zsi + wsi;

        float32v d3x = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), xsi );
        float32v d3y = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), ysi );
        float32v d3z = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), zsi );
        float32v d3w = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), wsi );

        xsi += float32v( 0.2f );
        ysi += float32v( 0.2f );
        zsi += float32v( 0.2f );
        wsi += float32v( 0.2f );
        sumsi += float32v( 0.8f );

        // 4
        mask32v v4x = xsi >= FS::Max( FS::Max( ysi, zsi ), FS::Max( wsi, float32v( 1 ) - sumsi ) );
        mask32v v4y = ysi >= FS::Max( FS::Max( zsi, wsi ), FS::Max( xsi, float32v( 1 ) - sumsi ) );
        mask32v v4z = zsi >= FS::Max( FS::Max( wsi, xsi ), FS::Max( ysi, float32v( 1 ) - sumsi ) );
        mask32v v4w = wsi >= FS::Max( FS::Max( xsi, ysi ), FS::Max( zsi, float32v( 1 ) - sumsi ) );

        int32v vh4x = FS::MaskedAdd( v4x, xsb, int32v( Primes::X ) );
        int32v vh4y = FS::MaskedAdd( v4y, ysb, int32v( Primes::Y ) );
        int32v vh4z = FS::MaskedAdd( v4z, zsb, int32v( Primes::Z ) );
        int32v vh4w = FS::MaskedAdd( v4w, wsb, int32v( Primes::W ) );

        xsi = FS::MaskedDecrement( v4x, xsi );
        ysi = FS::MaskedDecrement( v4y, ysi );
        zsi = FS::MaskedDecrement( v4z, zsi );
        wsi = FS::MaskedDecrement( v4w, wsi );
        sumsi = xsi + ysi + zsi + wsi;

        float32v d4x = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), xsi );
        float32v d4y = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), ysi );
        float32v d4z = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), zsi );
        float32v d4w = FS::FMulAdd( sumsi, float32v( 0.309016994374947f ), wsi );

        //float32v g0 = GetGradientDot(  )

        //vec4 h0123 = permute( mod639( vec4( vh0.x, vh1.x, vh2.x, vh3.x ) ), seedVec.x );
        //h0123 = permute( mod639( h0123 + vec4( vh0.y, vh1.y, vh2.y, vh3.y ) ), seedVec.y );
        //h0123 = permute( mod639( h0123 + vec4( vh0.z, vh1.z, vh2.z, vh3.z ) ), seedVec.z );
        //h0123 = pmod639( permute( mod639( h0123 + vec4( vh0.w, vh1.w, vh2.w, vh3.w ) ), seedVec.w ) );

        //float h4 = permute( mod639( vh4.x ), seedVec.x );
        //h4 = permute( mod639( h4 + vh4.y ), seedVec.y );
        //h4 = permute( mod639( h4 + vh4.z ), seedVec.z );
        //h4 = pmod639( permute( mod639( h4 + vh4.w ), seedVec.w ) );

        //vec4 g0 = grad( h0123.x );
        //vec4 g1 = grad( h0123.y );
        //vec4 g2 = grad( h0123.z );
        //vec4 g3 = grad( h0123.w );
        //vec4 g4 = grad( h4 );

        ///*vec4 norm = inversesqrt(vec4(dot(g0, g0), dot(g1, g1), dot(g2, g2), dot(g3, g3)));
        //g0 *= norm.x; g1 *= norm.y; g2 *= norm.z; g3 *= norm.w; g4 *= inversesqrt(dot(g4, g4));*/

        //// "Proper" constant is 0.5. 0.6 produces subtle invisible discontinuities, but looks much better.
        //vec4 a0123 = max( 0.6 - vec4( dot( d0, d0 ), dot( d1, d1 ), dot( d2, d2 ), dot( d3, d3 ) ), 0.0 );
        //float a4 = max( 0.6 - dot( d4, d4 ), 0.0 );
        //vec4 r0123 = vec4( dot( d0, g0 ), dot( d1, g1 ), dot( d2, g2 ), dot( d3, g3 ) );
        //float r4 = dot( d4, g4 );
        //a0123 *= a0123;
        //a4 *= a4;
        //return ( dot( a0123 * a0123, r0123 ) + a4 * a4 * r4 ) * 27.0; // TODO compute tighter normalization constant.
        return float32v( 0 );
    }
};

template<FastSIMD::FeatureSet SIMD>
class FastSIMD::DispatchClass<FastNoise::OpenSimplex2S, SIMD> final : public virtual FastNoise::OpenSimplex2S, public FastSIMD::DispatchClass<FastNoise::ScalableGenerator, SIMD>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        this->ScalePositions( x, y );

        const float SQRT3 = 1.7320508075688772935274463415059f;
        const float F2 = 0.5f * ( SQRT3 - 1.0f );
        const float G2 = ( SQRT3 - 3.0f ) / 6.0f;

        float32v s = float32v( F2 ) * ( x + y );
        float32v xs = x + s;
        float32v ys = y + s;
        float32v xsb = FS::Floor( xs );
        float32v ysb = FS::Floor( ys );
        float32v xsi = xs - xsb;
        float32v ysi = ys - ysb;
        int32v xsbp = FS::Convert<int32_t>( xsb ) * int32v( Primes::X );
        int32v ysbp = FS::Convert<int32_t>( ysb ) * int32v( Primes::Y );

        mask32v forwardXY = xsi + ysi > float32v( 1.0f );
        float32v boundaryXY = FS::Masked( forwardXY, float32v( -1.0f ) );
        mask32v forwardX = FS::FMulAdd( xsi, float32v( -2.0f ), ysi ) < boundaryXY;
        mask32v forwardY = FS::FMulAdd( ysi, float32v( -2.0f ), xsi ) < boundaryXY;

        float32v t = float32v( G2 ) * ( xsi + ysi );
        float32v xi = xsi + t;
        float32v yi = ysi + t;

        int32v h0 = HashPrimes( seed, xsbp, ysbp );
        float32v v0 = GetGradientDotFancy( h0, xi, yi );
        float32v a = FS::FNMulAdd( xi, xi, FS::FNMulAdd( yi, yi, float32v( 2.0f / 3.0f ) ) );
        float32v a0 = a; a0 *= a0; a0 *= a0;
        float32v value = a0 * v0;

        int32v h1 = HashPrimes( seed, xsbp + int32v( Primes::X ), ysbp + int32v( Primes::Y ) );
        float32v v1 = GetGradientDotFancy( h1, xi - float32v( 2 * G2 + 1 ), yi - float32v( 2 * G2 + 1 ) );
        float32v a1 = FS::FMulAdd( float32v( 2 * ( 1 + 2 * G2 ) * ( 1 / G2 + 2 ) ), t, a + float32v( -2 * ( 1 + 2 * G2 ) * ( 1 + 2 * G2 ) ) );
        a1 *= a1; a1 *= a1;
        value = FS::FMulAdd( a1, v1, value );

        float32v xyDelta = FS::Select( forwardXY, float32v( G2 + 1 ), float32v( -G2 ) );
        xi -= xyDelta;
        yi -= xyDelta;

        int32v h2 = HashPrimes( seed,
            FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardX, xsbp, int32v( Primes::X * 2 ) ), int32v( Primes::X ) ),
            FS::MaskedAdd( forwardXY, ysbp, int32v( Primes::Y ) ) );
        float32v xi2 = xi - FS::Select( forwardX, float32v( 1 + 2 * G2 ), float32v( -1 ) );
        float32v yi2 = FS::MaskedSub( forwardX, yi, float32v( 2 * G2 ) );
        float32v v2 = GetGradientDotFancy( h2, xi2, yi2 );
        float32v a2 = FS::Max( FS::FNMulAdd( xi2, xi2, FS::FNMulAdd( yi2, yi2, float32v( 2.0f / 3.0f ) ) ), float32v( 0 ) );
        a2 *= a2; a2 *= a2;
        value = FS::FMulAdd( a2, v2, value );

        int32v h3 = HashPrimes( seed,
            FS::MaskedAdd( forwardXY, xsbp, int32v( Primes::X ) ),
            FS::InvMaskedSub( forwardXY, FS::MaskedAdd( forwardY, ysbp, int32v( (int32_t)( Primes::Y * 2LL ) ) ), int32v( Primes::Y ) ) );
        float32v xi3 = FS::MaskedSub( forwardY, xi, float32v( 2 * G2 ) );
        float32v yi3 = yi - FS::Select( forwardY, float32v( 1 + 2 * G2 ), float32v( -1 ) );
        float32v v3 = GetGradientDotFancy( h3, xi3, yi3 );
        float32v a3 = FS::Max( FS::FNMulAdd( xi3, xi3, FS::FNMulAdd( yi3, yi3, float32v( 2.0f / 3.0f ) ) ), float32v( 0 ) );
        a3 *= a3; a3 *= a3;
        value = FS::FMulAdd( a3, v3, value );

        return float32v( 9.28993664146183f ) * value;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        this->ScalePositions( x, y, z );

        float32v f = float32v( 2.0f / 3.0f ) * ( x + y + z );
        float32v xr = f - x;
        float32v yr = f - y;
        float32v zr = f - z;

        float32v xrb = FS::Floor( xr );
        float32v yrb = FS::Floor( yr );
        float32v zrb = FS::Floor( zr );
        float32v xri = xr - xrb;
        float32v yri = yr - yrb;
        float32v zri = zr - zrb;
        int32v xrbp = FS::Convert<int32_t>( xrb ) * int32v( Primes::X );
        int32v yrbp = FS::Convert<int32_t>( yrb ) * int32v( Primes::Y );
        int32v zrbp = FS::Convert<int32_t>( zrb ) * int32v( Primes::Z );

        float32v value( 0 );
        for( size_t i = 0; ; i++ )
        {
            float32v a = FS::FNMulAdd( xri, xri, FS::FNMulAdd( yri, yri, FS::FNMulAdd( zri, zri, float32v( 0.75f ) ) ) ) * float32v( 0.5f );

            float32v p0 = zri + yri + xri - float32v( 1.5f );
            mask32v flip0 = p0 >= float32v( 0.0f );
            float32v a0 = FS::Max( FS::MaskedAdd( flip0, a, p0 ), float32v( 0 ) );
            a0 *= a0; a0 *= a0;
            int32v h0 = HashPrimes( seed, FS::MaskedAdd( flip0, xrbp, int32v( Primes::X ) ), FS::MaskedAdd( flip0, yrbp, int32v( Primes::Y )), FS::MaskedAdd( flip0, zrbp, int32v( Primes::Z )));
            float32v v0 = GetGradientDot( h0, FS::MaskedDecrement( flip0, xri ), FS::MaskedDecrement( flip0, yri ), FS::MaskedDecrement( flip0, zri ) );
            value = FS::FMulAdd( a0, v0, value );
            a -= float32v( 0.5f );

            float32v p1 = zri + yri - xri + float32v( -0.5f );
            mask32v flip1 = p1 >= float32v( 0.0f );
            float32v a1 = FS::Max( FS::MaskedAdd( flip1, a + xri, p1 ), float32v( 0 ) );
            a1 *= a1; a1 *= a1;
            int32v h1 = HashPrimes( seed, FS::InvMaskedAdd( flip1, xrbp, int32v( Primes::X )), FS::MaskedAdd( flip1, yrbp, int32v( Primes::Y ) ), FS::MaskedAdd( flip1, zrbp, int32v( Primes::Z )));
            float32v v1 = GetGradientDot( h1, FS::InvMaskedSub( flip1, xri, float32v( 1.0f ) ), FS::MaskedDecrement( flip1, yri ), FS::MaskedDecrement( flip1, zri ) );
            value = FS::FMulAdd( a1, v1, value );

            float32v p2 = xri + float32v( -0.5f ) + ( zri - yri );
            mask32v flip2 = p2 >= float32v( 0.0f );
            float32v a2 = FS::Max( FS::MaskedAdd( flip2, a + yri, p2 ), float32v( 0 ) );
            a2 *= a2; a2 *= a2;
            int32v h2 = HashPrimes( seed, FS::MaskedAdd( flip2, xrbp, int32v( Primes::X )), FS::InvMaskedAdd( flip2, yrbp, int32v( Primes::Y )), FS::MaskedAdd( flip2, zrbp, int32v( Primes::Z )));
            float32v v2 = GetGradientDot( h2, FS::MaskedDecrement( flip2, xri ), FS::InvMaskedSub( flip2, yri, float32v( 1.0f ) ), FS::MaskedDecrement( flip2, zri ) );
            value = FS::FMulAdd( a2, v2, value );

            float32v p3 = xri + float32v( -0.5f ) - ( zri - yri );
            mask32v flip3 = p3 >= float32v( 0.0f );
            float32v a3 = FS::Max( FS::MaskedAdd( flip3, a + zri, p3 ), float32v( 0 ) );
            a3 *= a3; a3 *= a3;
            int32v h3 = HashPrimes( seed, FS::MaskedAdd( flip3, xrbp, int32v( Primes::X )), FS::MaskedAdd( flip3, yrbp, int32v( Primes::Y )), FS::InvMaskedAdd( flip3, zrbp, int32v( Primes::Z )));
            float32v v3 = GetGradientDot( h3, FS::MaskedDecrement( flip3, xri ), FS::MaskedDecrement( flip3, yri ), FS::InvMaskedSub( flip3, zri, float32v( 1.0f ) ) );
            value = FS::FMulAdd( a3, v3, value );

            if( i == 1 )
            {
                break;
            }

            mask32v sideX = xri >= float32v( 0.5f );
            mask32v sideY = yri >= float32v( 0.5f );
            mask32v sideZ = zri >= float32v( 0.5f );

            xrbp = FS::MaskedAdd( sideX, xrbp, int32v( Primes::X ) );
            yrbp = FS::MaskedAdd( sideY, yrbp, int32v( Primes::Y ) );
            zrbp = FS::MaskedAdd( sideZ, zrbp, int32v( Primes::Z ) );

            xri += FS::Select( sideX, float32v( -0.5f ), float32v( 0.5f ) );
            yri += FS::Select( sideY, float32v( -0.5f ), float32v( 0.5f ) );
            zri += FS::Select( sideZ, float32v( -0.5f ), float32v( 0.5f ) );

            seed = ~seed;
        }

        return float32v( 144.736422163332608f ) * value;
    }
};

