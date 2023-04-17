#include "FastSIMD/InlInclude.h"

#include "Simplex.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Simplex, FS> : public virtual FastNoise::Simplex, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        const float SQRT3 = 1.7320508075688772935274463415059f;
        const float F2 = 0.5f * (SQRT3 - 1.0f);
        const float G2 = (3.0f - SQRT3) / 6.0f;

        float32v f = float32v( F2 ) * (x + y);
        float32v x0 = FS_Floor_f32( x + f );
        float32v y0 = FS_Floor_f32( y + f );

        int32v i = FS_Convertf32_i32( x0 ) * int32v( FnPrimes::X );
        int32v j = FS_Convertf32_i32( y0 ) * int32v( FnPrimes::Y );

        float32v g = float32v( G2 ) * (x0 + y0);
        x0 = x - (x0 - g);
        y0 = y - (y0 - g);

        mask32v i1 = x0 > y0;
        //mask32v j1 = ~i1; //NMasked funcs

        float32v x1 = FS_MaskedSub_f32( x0, float32v( 1.f ), i1 ) + float32v( G2 );
        float32v y1 = FS_NMaskedSub_f32( y0, float32v( 1.f ), i1 ) + float32v( G2 );

        float32v x2 = x0 + float32v( G2 * 2 - 1 );
        float32v y2 = y0 + float32v( G2 * 2 - 1 );

        float32v t0 = FS_FNMulAdd_f32( x0, x0, FS_FNMulAdd_f32( y0, y0, float32v( 0.5f ) ) );
        float32v t1 = FS_FNMulAdd_f32( x1, x1, FS_FNMulAdd_f32( y1, y1, float32v( 0.5f ) ) );
        float32v t2 = FS_FNMulAdd_f32( x2, x2, FS_FNMulAdd_f32( y2, y2, float32v( 0.5f ) ) );

        t0 = FS_Max_f32( t0, float32v( 0 ) );
        t1 = FS_Max_f32( t1, float32v( 0 ) );
        t2 = FS_Max_f32( t2, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;

        float32v n0 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i, j ), x0, y0 );
        float32v n1 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i1 ), FS_NMaskedAdd_i32( j, int32v( FnPrimes::Y ), i1 ) ), x1, y1 );
        float32v n2 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i + int32v( FnPrimes::X ), j + int32v( FnPrimes::Y ) ), x2, y2 );

        return float32v( 38.283687591552734375f ) * FS_FMulAdd_f32( n0, t0, FS_FMulAdd_f32( n1, t1, n2 * t2 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        const float F3 = 1.0f / 3.0f;
        const float G3 = 1.0f / 2.0f;

        float32v s = float32v( F3 ) * (x + y + z);
        x += s;
        y += s;
        z += s;

        float32v x0 = FS_Floor_f32( x );
        float32v y0 = FS_Floor_f32( y );
        float32v z0 = FS_Floor_f32( z );
        float32v xi = x - x0;
        float32v yi = y - y0;
        float32v zi = z - z0;

        int32v i = FS_Convertf32_i32( x0 ) * int32v( FnPrimes::X );
        int32v j = FS_Convertf32_i32( y0 ) * int32v( FnPrimes::Y );
        int32v k = FS_Convertf32_i32( z0 ) * int32v( FnPrimes::Z );

        mask32v x_ge_y = xi >= yi;
        mask32v y_ge_z = yi >= zi;
        mask32v x_ge_z = xi >= zi;

        float32v g = float32v( G3 ) * (xi + yi + zi);
        x0 = xi - g;
        y0 = yi - g;
        z0 = zi - g;

        mask32v i1 = x_ge_y & x_ge_z;
        mask32v j1 = FS_BitwiseAndNot_m32( y_ge_z, x_ge_y );
        mask32v k1 = FS_BitwiseAndNot_m32( ~x_ge_z, y_ge_z );

        mask32v i2 = x_ge_y | x_ge_z;
        mask32v j2 = ~x_ge_y | y_ge_z;
        mask32v k2 = x_ge_z & y_ge_z; //NMasked

        float32v x1 = FS_MaskedSub_f32( x0, float32v( 1 ), i1 ) + float32v( G3 );
        float32v y1 = FS_MaskedSub_f32( y0, float32v( 1 ), j1 ) + float32v( G3 );
        float32v z1 = FS_MaskedSub_f32( z0, float32v( 1 ), k1 ) + float32v( G3 );
        float32v x2 = FS_MaskedSub_f32( x0, float32v( 1 ), i2 ) + float32v( G3 * 2 );
        float32v y2 = FS_MaskedSub_f32( y0, float32v( 1 ), j2 ) + float32v( G3 * 2 );
        float32v z2 = FS_NMaskedSub_f32( z0, float32v( 1 ), k2 ) + float32v( G3 * 2 );
        float32v x3 = x0 + float32v( G3 * 3 - 1 );
        float32v y3 = y0 + float32v( G3 * 3 - 1 );
        float32v z3 = z0 + float32v( G3 * 3 - 1 );

        float32v t0 = FS_FNMulAdd_f32( x0, x0, FS_FNMulAdd_f32( y0, y0, FS_FNMulAdd_f32( z0, z0, float32v( 0.6f ) ) ) );
        float32v t1 = FS_FNMulAdd_f32( x1, x1, FS_FNMulAdd_f32( y1, y1, FS_FNMulAdd_f32( z1, z1, float32v( 0.6f ) ) ) );
        float32v t2 = FS_FNMulAdd_f32( x2, x2, FS_FNMulAdd_f32( y2, y2, FS_FNMulAdd_f32( z2, z2, float32v( 0.6f ) ) ) );
        float32v t3 = FS_FNMulAdd_f32( x3, x3, FS_FNMulAdd_f32( y3, y3, FS_FNMulAdd_f32( z3, z3, float32v( 0.6f ) ) ) );

        t0 = FS_Max_f32( t0, float32v( 0 ) );
        t1 = FS_Max_f32( t1, float32v( 0 ) );
        t2 = FS_Max_f32( t2, float32v( 0 ) );
        t3 = FS_Max_f32( t3, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;
        t3 *= t3; t3 *= t3;             

        float32v n0 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i, j, k ), x0, y0, z0 );
        float32v n1 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i1 ), FS_MaskedAdd_i32( j, int32v( FnPrimes::Y ), j1 ), FS_MaskedAdd_i32( k, int32v( FnPrimes::Z ), k1 ) ), x1, y1, z1 );
        float32v n2 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i2 ), FS_MaskedAdd_i32( j, int32v( FnPrimes::Y ), j2 ), FS_NMaskedAdd_i32( k, int32v( FnPrimes::Z ), k2 ) ), x2, y2, z2 );
        float32v n3 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i + int32v( FnPrimes::X ), j + int32v( FnPrimes::Y ), k + int32v( FnPrimes::Z ) ), x3, y3, z3 );

        return float32v( 32.69428253173828125f ) * FS_FMulAdd_f32( n0, t0, FS_FMulAdd_f32( n1, t1, FS_FMulAdd_f32( n2, t2, n3 * t3 ) ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        const float SQRT5 = 2.236067977499f;
        const float F4 = (SQRT5 - 1.0f) / 4.0f;
        const float G4 = (5.0f - SQRT5) / 20.0f;

        float32v s = float32v( F4 ) * (x + y + z + w);
        x += s;
        y += s;
        z += s;
        w += s;

        float32v x0 = FS_Floor_f32( x );
        float32v y0 = FS_Floor_f32( y );
        float32v z0 = FS_Floor_f32( z );
        float32v w0 = FS_Floor_f32( w );
        float32v xi = x - x0;
        float32v yi = y - y0;
        float32v zi = z - z0;
        float32v wi = w - w0;

        int32v i = FS_Convertf32_i32( x0 ) * int32v( FnPrimes::X );
        int32v j = FS_Convertf32_i32( y0 ) * int32v( FnPrimes::Y );
        int32v k = FS_Convertf32_i32( z0 ) * int32v( FnPrimes::Z );
        int32v l = FS_Convertf32_i32( w0 ) * int32v( FnPrimes::W );

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
        rankx = FS_MaskedIncrement_i32( rankx, x_ge_y );
        ranky = FS_MaskedIncrement_i32( ranky, ~x_ge_y );

        mask32v x_ge_z = x0 >= z0;
        rankx = FS_MaskedIncrement_i32( rankx, x_ge_z );
        rankz = FS_MaskedIncrement_i32( rankz, ~x_ge_z );

        mask32v x_ge_w = x0 >= w0;
        rankx = FS_MaskedIncrement_i32( rankx, x_ge_w );
        rankw = FS_MaskedIncrement_i32( rankw, ~x_ge_w );

        mask32v y_ge_z = y0 >= z0;
        ranky = FS_MaskedIncrement_i32( ranky, y_ge_z );
        rankz = FS_MaskedIncrement_i32( rankz, ~y_ge_z );

        mask32v y_ge_w = y0 >= w0;
        ranky = FS_MaskedIncrement_i32( ranky, y_ge_w );
        rankw = FS_MaskedIncrement_i32( rankw, ~y_ge_w );

        mask32v z_ge_w = z0 >= w0;
        rankz = FS_MaskedIncrement_i32( rankz, z_ge_w );
        rankw = FS_MaskedIncrement_i32( rankw, ~z_ge_w );

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

        float32v x1 = FS_MaskedSub_f32( x0, float32v( 1 ), i1 ) + float32v( G4 );
        float32v y1 = FS_MaskedSub_f32( y0, float32v( 1 ), j1 ) + float32v( G4 );
        float32v z1 = FS_MaskedSub_f32( z0, float32v( 1 ), k1 ) + float32v( G4 );
        float32v w1 = FS_MaskedSub_f32( w0, float32v( 1 ), l1 ) + float32v( G4 );
        float32v x2 = FS_MaskedSub_f32( x0, float32v( 1 ), i2 ) + float32v( G4 * 2 );
        float32v y2 = FS_MaskedSub_f32( y0, float32v( 1 ), j2 ) + float32v( G4 * 2 );
        float32v z2 = FS_MaskedSub_f32( z0, float32v( 1 ), k2 ) + float32v( G4 * 2 );
        float32v w2 = FS_MaskedSub_f32( w0, float32v( 1 ), l2 ) + float32v( G4 * 2 );
        float32v x3 = FS_MaskedSub_f32( x0, float32v( 1 ), i3 ) + float32v( G4 * 3 );
        float32v y3 = FS_MaskedSub_f32( y0, float32v( 1 ), j3 ) + float32v( G4 * 3 );
        float32v z3 = FS_MaskedSub_f32( z0, float32v( 1 ), k3 ) + float32v( G4 * 3 );
        float32v w3 = FS_MaskedSub_f32( w0, float32v( 1 ), l3 ) + float32v( G4 * 3 );
        float32v x4 = x0 + float32v( G4 * 4 - 1 );
        float32v y4 = y0 + float32v( G4 * 4 - 1 );
        float32v z4 = z0 + float32v( G4 * 4 - 1 );
        float32v w4 = w0 + float32v( G4 * 4 - 1 );

        float32v t0 = FS_FNMulAdd_f32( x0, x0, FS_FNMulAdd_f32( y0, y0, FS_FNMulAdd_f32( z0, z0, FS_FNMulAdd_f32( w0, w0, float32v( 0.6f ) ) ) ) );
        float32v t1 = FS_FNMulAdd_f32( x1, x1, FS_FNMulAdd_f32( y1, y1, FS_FNMulAdd_f32( z1, z1, FS_FNMulAdd_f32( w1, w1, float32v( 0.6f ) ) ) ) );
        float32v t2 = FS_FNMulAdd_f32( x2, x2, FS_FNMulAdd_f32( y2, y2, FS_FNMulAdd_f32( z2, z2, FS_FNMulAdd_f32( w2, w2, float32v( 0.6f ) ) ) ) );
        float32v t3 = FS_FNMulAdd_f32( x3, x3, FS_FNMulAdd_f32( y3, y3, FS_FNMulAdd_f32( z3, z3, FS_FNMulAdd_f32( w3, w3, float32v( 0.6f ) ) ) ) );
        float32v t4 = FS_FNMulAdd_f32( x4, x4, FS_FNMulAdd_f32( y4, y4, FS_FNMulAdd_f32( z4, z4, FS_FNMulAdd_f32( w4, w4, float32v( 0.6f ) ) ) ) );

        t0 = FS_Max_f32( t0, float32v( 0 ) );
        t1 = FS_Max_f32( t1, float32v( 0 ) );
        t2 = FS_Max_f32( t2, float32v( 0 ) );
        t3 = FS_Max_f32( t3, float32v( 0 ) );
        t4 = FS_Max_f32( t4, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;
        t3 *= t3; t3 *= t3;
        t4 *= t4; t4 *= t4;

        float32v n0 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i, j, k, l ), x0, y0, z0, w0 );
        float32v n1 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, 
            FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i1 ),
            FS_MaskedAdd_i32( j, int32v( FnPrimes::Y ), j1 ),
            FS_MaskedAdd_i32( k, int32v( FnPrimes::Z ), k1 ),
            FS_MaskedAdd_i32( l, int32v( FnPrimes::W ), l1 ) ), x1, y1, z1, w1 );
        float32v n2 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, 
            FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i2 ),
            FS_MaskedAdd_i32( j, int32v( FnPrimes::Y ), j2 ),
            FS_MaskedAdd_i32( k, int32v( FnPrimes::Z ), k2 ),
            FS_MaskedAdd_i32( l, int32v( FnPrimes::W ), l2 ) ), x2, y2, z2, w2 );
        float32v n3 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed,
            FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i3 ),
            FS_MaskedAdd_i32( j, int32v( FnPrimes::Y ), j3 ),
            FS_MaskedAdd_i32( k, int32v( FnPrimes::Z ), k3 ),
            FS_MaskedAdd_i32( l, int32v( FnPrimes::W ), l3 ) ), x3, y3, z3, w3 );
        float32v n4 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, i + int32v( FnPrimes::X ), j + int32v( FnPrimes::Y ), k + int32v( FnPrimes::Z ), l + int32v( FnPrimes::W ) ), x4, y4, z4, w4 );

        return float32v( 27.f ) * FS_FMulAdd_f32( n0, t0, FS_FMulAdd_f32( n1, t1, FS_FMulAdd_f32( n2, t2, FS_FMulAdd_f32( n3, t3, n4 * t4 ) ) ) );
    }
};

template<typename FS>
class FS_T<FastNoise::OpenSimplex2, FS> : public virtual FastNoise::OpenSimplex2, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        const float SQRT3 = 1.7320508075f;
        const float F2 = 0.5f * (SQRT3 - 1.0f);
        const float G2 = (3.0f - SQRT3) / 6.0f;

        float32v f = float32v( F2 ) * (x + y);
        float32v x0 = FS_Floor_f32( x + f );
        float32v y0 = FS_Floor_f32( y + f );

        int32v i = FS_Convertf32_i32( x0 ) * int32v( FnPrimes::X );
        int32v j = FS_Convertf32_i32( y0 ) * int32v( FnPrimes::Y );

        float32v g = float32v( G2 ) * (x0 + y0);
        x0 = x - (x0 - g);
        y0 = y - (y0 - g);

        mask32v i1 = x0 > y0;
        //mask32v j1 = ~i1; //NMasked funcs

        float32v x1 = FS_MaskedSub_f32( x0, float32v( 1.f ), i1 ) + float32v( G2 );
        float32v y1 = FS_NMaskedSub_f32( y0, float32v( 1.f ), i1 ) + float32v( G2 );
        float32v x2 = x0 + float32v( (G2 * 2) - 1 );
        float32v y2 = y0 + float32v( (G2 * 2) - 1 );

        float32v t0 = float32v( 0.5f ) - (x0 * x0) - (y0 * y0);
        float32v t1 = float32v( 0.5f ) - (x1 * x1) - (y1 * y1);
        float32v t2 = float32v( 0.5f ) - (x2 * x2) - (y2 * y2);

        t0 = FS_Max_f32( t0, float32v( 0 ) );
        t1 = FS_Max_f32( t1, float32v( 0 ) );
        t2 = FS_Max_f32( t2, float32v( 0 ) );

        t0 *= t0; t0 *= t0;
        t1 *= t1; t1 *= t1;
        t2 *= t2; t2 *= t2;

        float32v n0 = FnUtils::GetGradientDotFancy( FnUtils::HashPrimes( seed, i, j ), x0, y0 );
        float32v n1 = FnUtils::GetGradientDotFancy( FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( i, int32v( FnPrimes::X ), i1 ), FS_NMaskedAdd_i32( j, int32v( FnPrimes::Y ), i1 ) ), x1, y1 );
        float32v n2 = FnUtils::GetGradientDotFancy( FnUtils::HashPrimes( seed, i + int32v( FnPrimes::X ), j + int32v( FnPrimes::Y ) ), x2, y2 );

        return float32v( 49.918426513671875f ) * FS_FMulAdd_f32( n0, t0, FS_FMulAdd_f32( n1, t1, n2 * t2 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v f = float32v( 2.0f / 3.0f ) * (x + y + z);
        float32v xr = f - x;
        float32v yr = f - y;
        float32v zr = f - z;

        float32v val( 0 );
        for( size_t i = 0; ; i++ )
        {
            float32v v0xr = FS_Round_f32( xr );
            float32v v0yr = FS_Round_f32( yr );
            float32v v0zr = FS_Round_f32( zr );
            float32v d0xr = xr - v0xr;
            float32v d0yr = yr - v0yr;
            float32v d0zr = zr - v0zr;

            float32v score0xr = FS_Abs_f32( d0xr );
            float32v score0yr = FS_Abs_f32( d0yr );
            float32v score0zr = FS_Abs_f32( d0zr );
            mask32v dir0xr = FS_Max_f32( score0yr, score0zr ) <= score0xr;
            mask32v dir0yr = FS_BitwiseAndNot_m32( FS_Max_f32( score0zr, score0xr ) <= score0yr, dir0xr );
            mask32v dir0zr = ~(dir0xr | dir0yr);
            float32v v1xr = FS_MaskedAdd_f32( v0xr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0xr ), dir0xr );
            float32v v1yr = FS_MaskedAdd_f32( v0yr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0yr ), dir0yr );
            float32v v1zr = FS_MaskedAdd_f32( v0zr, float32v( 1.0f ) | ( float32v( -1.0f ) & d0zr ), dir0zr );
            float32v d1xr = xr - v1xr;
            float32v d1yr = yr - v1yr;
            float32v d1zr = zr - v1zr;

            int32v hv0xr = FS_Convertf32_i32( v0xr ) * int32v( FnPrimes::X );
            int32v hv0yr = FS_Convertf32_i32( v0yr ) * int32v( FnPrimes::Y );
            int32v hv0zr = FS_Convertf32_i32( v0zr ) * int32v( FnPrimes::Z );

            int32v hv1xr = FS_Convertf32_i32( v1xr ) * int32v( FnPrimes::X );
            int32v hv1yr = FS_Convertf32_i32( v1yr ) * int32v( FnPrimes::Y );
            int32v hv1zr = FS_Convertf32_i32( v1zr ) * int32v( FnPrimes::Z );

            float32v t0 = FS_FNMulAdd_f32( d0zr, d0zr, FS_FNMulAdd_f32( d0yr, d0yr, FS_FNMulAdd_f32( d0xr, d0xr, float32v( 0.6f ) ) ) );
            float32v t1 = FS_FNMulAdd_f32( d1zr, d1zr, FS_FNMulAdd_f32( d1yr, d1yr, FS_FNMulAdd_f32( d1xr, d1xr, float32v( 0.6f ) ) ) );
            t0 = FS_Max_f32( t0, float32v( 0 ) );
            t1 = FS_Max_f32( t1, float32v( 0 ) );
            t0 *= t0; t0 *= t0;
            t1 *= t1; t1 *= t1;

            float32v v0 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, hv0xr, hv0yr, hv0zr ), d0xr, d0yr, d0zr );
            float32v v1 = FnUtils::GetGradientDot( FnUtils::HashPrimes( seed, hv1xr, hv1yr, hv1zr ), d1xr, d1yr, d1zr );

            val = FS_FMulAdd_f32( v0, t0, FS_FMulAdd_f32( v1, t1, val ) );

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
};

template<typename FS>
class FS_T<FastNoise::OpenSimplex2S, FS> : public virtual FastNoise::OpenSimplex2S, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        const float SQRT3 = 1.7320508075688772935274463415059f;
        const float F2 = 0.5f * ( SQRT3 - 1.0f );
        const float G2 = ( SQRT3 - 3.0f ) / 6.0f;

        float32v s = float32v( F2 ) * ( x + y );
        float32v xs = x + s;
        float32v ys = y + s;
        float32v xsb = FS_Floor_f32( xs );
        float32v ysb = FS_Floor_f32( ys );
        float32v xsi = xs - xsb;
        float32v ysi = ys - ysb;
        int32v xsbp = FS_Convertf32_i32( xsb ) * int32v( FnPrimes::X );
        int32v ysbp = FS_Convertf32_i32( ysb ) * int32v( FnPrimes::Y );

        mask32v forwardXY = xsi + ysi > float32v( 1.0f );
        float32v boundaryXY = FS_Mask_f32( float32v( -1.0f ), forwardXY );
        mask32v forwardX = FS_FMulAdd_f32( xsi, float32v( -2.0f ), ysi ) < boundaryXY;
        mask32v forwardY = FS_FMulAdd_f32( ysi, float32v( -2.0f ), xsi ) < boundaryXY;

        float32v t = float32v( G2 ) * ( xsi + ysi );
        float32v xi = xsi + t;
        float32v yi = ysi + t;

        int32v h0 = FnUtils::HashPrimes( seed, xsbp, ysbp );
        float32v v0 = FnUtils::GetGradientDotFancy( h0, xi, yi );
        float32v a = FS_FNMulAdd_f32( xi, xi, FS_FNMulAdd_f32( yi, yi, float32v( 2.0f / 3.0f ) ) );
        float32v a0 = a; a0 *= a0; a0 *= a0;
        float32v value = a0 * v0;

        int32v h1 = FnUtils::HashPrimes( seed, xsbp + int32v( FnPrimes::X ), ysbp + int32v( FnPrimes::Y ) );
        float32v v1 = FnUtils::GetGradientDotFancy( h1, xi - float32v( 2 * G2 + 1 ), yi - float32v( 2 * G2 + 1 ) );
        float32v a1 = FS_FMulAdd_f32( float32v( 2 * ( 1 + 2 * G2 ) * ( 1 / G2 + 2 ) ), t, a + float32v( -2 * ( 1 + 2 * G2 ) * ( 1 + 2 * G2 ) ) );
        a1 *= a1; a1 *= a1;
        value = FS_FMulAdd_f32( a1, v1, value );

        float32v xyDelta = FS_Select_f32( forwardXY, float32v( G2 + 1 ), float32v( -G2 ) );
        xi -= xyDelta;
        yi -= xyDelta;

        int32v h2 = FnUtils::HashPrimes( seed,
            FS_NMaskedSub_i32( FS_MaskedAdd_i32( xsbp, int32v( FnPrimes::X * 2 ), forwardX ), int32v( FnPrimes::X ), forwardXY ),
            FS_MaskedAdd_i32( ysbp, int32v( FnPrimes::Y ), forwardXY ) );
        float32v xi2 = xi - FS_Select_f32( forwardX, float32v( 1 + 2 * G2 ), float32v( -1 ) );
        float32v yi2 = FS_MaskedSub_f32( yi, float32v( 2 * G2 ), forwardX );
        float32v v2 = FnUtils::GetGradientDotFancy( h2, xi2, yi2 );
        float32v a2 = FS_Max_f32( FS_FNMulAdd_f32( xi2, xi2, FS_FNMulAdd_f32( yi2, yi2, float32v( 2.0f / 3.0f ) ) ), float32v( 0 ) );
        a2 *= a2; a2 *= a2;
        value = FS_FMulAdd_f32( a2, v2, value );

        int32v h3 = FnUtils::HashPrimes( seed,
            FS_MaskedAdd_i32( xsbp, int32v( FnPrimes::X ), forwardXY ),
            FS_NMaskedSub_i32( FS_MaskedAdd_i32( ysbp, int32v( FnPrimes::Y * 2 ), forwardY ), int32v( FnPrimes::Y ), forwardXY ) );
        float32v xi3 = FS_MaskedSub_f32( xi, float32v( 2 * G2 ), forwardY );
        float32v yi3 = yi - FS_Select_f32( forwardY, float32v( 1 + 2 * G2 ), float32v( -1 ) );
        float32v v3 = FnUtils::GetGradientDotFancy( h3, xi3, yi3 );
        float32v a3 = FS_Max_f32( FS_FNMulAdd_f32( xi3, xi3, FS_FNMulAdd_f32( yi3, yi3, float32v( 2.0f / 3.0f ) ) ), float32v( 0 ) );
        a3 *= a3; a3 *= a3;
        value = FS_FMulAdd_f32( a3, v3, value );

        return float32v( 9.28993664146183f ) * value;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v f = float32v( 2.0f / 3.0f ) * ( x + y + z );
        float32v xr = f - x;
        float32v yr = f - y;
        float32v zr = f - z;

        float32v xrb = FS_Floor_f32( xr );
        float32v yrb = FS_Floor_f32( yr );
        float32v zrb = FS_Floor_f32( zr );
        float32v xri = xr - xrb;
        float32v yri = yr - yrb;
        float32v zri = zr - zrb;
        int32v xrbp = FS_Convertf32_i32( xrb ) * int32v( FnPrimes::X );
        int32v yrbp = FS_Convertf32_i32( yrb ) * int32v( FnPrimes::Y );
        int32v zrbp = FS_Convertf32_i32( zrb ) * int32v( FnPrimes::Z );

        float32v value( 0 );
        for( size_t i = 0; ; i++ )
        {
            float32v a = FS_FNMulAdd_f32( xri, xri, FS_FNMulAdd_f32( yri, yri, FS_FNMulAdd_f32( zri, zri, float32v( 0.75f ) ) ) ) * float32v( 0.5f );

            float32v p0 = zri + yri + xri - float32v( 1.5f );
            mask32v flip0 = p0 >= float32v( 0.0f );
            float32v a0 = FS_Max_f32( FS_MaskedAdd_f32( a, p0, flip0 ), float32v( 0 ) );
            a0 *= a0; a0 *= a0;
            int32v h0 = FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( xrbp, int32v( FnPrimes::X ), flip0 ), FS_MaskedAdd_i32( yrbp, int32v( FnPrimes::Y ), flip0 ), FS_MaskedAdd_i32( zrbp, int32v( FnPrimes::Z ), flip0 ) );
            float32v v0 = FnUtils::GetGradientDot( h0, FS_MaskedSub_f32( xri, float32v( 1.0f ), flip0 ), FS_MaskedSub_f32( yri, float32v( 1.0f ), flip0 ), FS_MaskedSub_f32( zri, float32v( 1.0f ), flip0 ) );
            value = FS_FMulAdd_f32( a0, v0, value );
            a -= float32v( 0.5f );

            float32v p1 = zri + yri - xri + float32v( -0.5f );
            mask32v flip1 = p1 >= float32v( 0.0f );
            float32v a1 = FS_Max_f32( FS_MaskedAdd_f32( a + xri, p1, flip1 ), float32v( 0 ) );
            a1 *= a1; a1 *= a1;
            int32v h1 = FnUtils::HashPrimes( seed, FS_NMaskedAdd_i32( xrbp, int32v( FnPrimes::X ), flip1 ), FS_MaskedAdd_i32( yrbp, int32v( FnPrimes::Y ), flip1 ), FS_MaskedAdd_i32( zrbp, int32v( FnPrimes::Z ), flip1 ) );
            float32v v1 = FnUtils::GetGradientDot( h1, FS_NMaskedSub_f32( xri, float32v( 1.0f ), flip1 ), FS_MaskedSub_f32( yri, float32v( 1.0f ), flip1 ), FS_MaskedSub_f32( zri, float32v( 1.0f ), flip1 ) );
            value = FS_FMulAdd_f32( a1, v1, value );

            float32v p2 = xri + float32v( -0.5f ) + ( zri - yri );
            mask32v flip2 = p2 >= float32v( 0.0f );
            float32v a2 = FS_Max_f32( FS_MaskedAdd_f32( a + yri, p2, flip2 ), float32v( 0 ) );
            a2 *= a2; a2 *= a2;
            int32v h2 = FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( xrbp, int32v( FnPrimes::X ), flip2 ), FS_NMaskedAdd_i32( yrbp, int32v( FnPrimes::Y ), flip2 ), FS_MaskedAdd_i32( zrbp, int32v( FnPrimes::Z ), flip2 ) );
            float32v v2 = FnUtils::GetGradientDot( h2, FS_MaskedSub_f32( xri, float32v( 1.0f ), flip2 ), FS_NMaskedSub_f32( yri, float32v( 1.0f ), flip2 ), FS_MaskedSub_f32( zri, float32v( 1.0f ), flip2 ) );
            value = FS_FMulAdd_f32( a2, v2, value );

            float32v p3 = xri + float32v( -0.5f ) - ( zri - yri );
            mask32v flip3 = p3 >= float32v( 0.0f );
            float32v a3 = FS_Max_f32( FS_MaskedAdd_f32( a + zri, p3, flip3 ), float32v( 0 ) );
            a3 *= a3; a3 *= a3;
            int32v h3 = FnUtils::HashPrimes( seed, FS_MaskedAdd_i32( xrbp, int32v( FnPrimes::X ), flip3 ), FS_MaskedAdd_i32( yrbp, int32v( FnPrimes::Y ), flip3 ), FS_NMaskedAdd_i32( zrbp, int32v( FnPrimes::Z ), flip3 ) );
            float32v v3 = FnUtils::GetGradientDot( h3, FS_MaskedSub_f32( xri, float32v( 1.0f ), flip3 ), FS_MaskedSub_f32( yri, float32v( 1.0f ), flip3 ), FS_NMaskedSub_f32( zri, float32v( 1.0f ), flip3 ) );
            value = FS_FMulAdd_f32( a3, v3, value );

            if( i == 1 )
            {
                break;
            }

            mask32v sideX = xri >= float32v( 0.5f );
            mask32v sideY = yri >= float32v( 0.5f );
            mask32v sideZ = zri >= float32v( 0.5f );

            xrbp = FS_MaskedAdd_i32( xrbp, int32v( FnPrimes::X ), sideX );
            yrbp = FS_MaskedAdd_i32( yrbp, int32v( FnPrimes::Y ), sideY );
            zrbp = FS_MaskedAdd_i32( zrbp, int32v( FnPrimes::Z ), sideZ );

            xri += FS_Select_f32( sideX, float32v( -0.5f ), float32v( 0.5f ) );
            yri += FS_Select_f32( sideY, float32v( -0.5f ), float32v( 0.5f ) );
            zri += FS_Select_f32( sideZ, float32v( -0.5f ), float32v( 0.5f ) );

            seed = ~seed;
        }

        return float32v( 144.736422163332608f ) * value;
    }
};

