#define FASTSIMD_INTELLISENSE
#include "Perlin.h"

template<typename F, FastSIMD::eLevel S>
typename FS_CLASS( FastNoise::Perlin )<F, S>::float32v
FS_CLASS( FastNoise::Perlin )<F, S>::Gen( int32v seed, float32v x, float32v y )
{
    float32v xs = FS_Floor_f32( x );
    float32v ys = FS_Floor_f32( y );

    int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
    int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
    int32v x1 = x0 + int32v( Primes::X );
    int32v y1 = y0 + int32v( Primes::Y );

    float32v xf0 = xs = x - xs;
    float32v yf0 = ys = y - ys;
    float32v xf1 = xf0 - float32v( 1 );
    float32v yf1 = yf0 - float32v( 1 );

    xs = this->InterpQuintic( xs );
    ys = this->InterpQuintic( ys );

    return this->Lerp(
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y0 ), xf0, yf0 ), this->GetGradientDot( this->HashPrimes( seed, x1, y0 ), xf1, yf0 ), xs),
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y1 ), xf0, yf1 ), this->GetGradientDot( this->HashPrimes( seed, x1, y1 ), xf1, yf1 ), xs), ys);
}

template<typename F, FastSIMD::eLevel S>
typename FS_CLASS( FastNoise::Perlin )<F, S>::float32v
FS_CLASS( FastNoise::Perlin )<F, S>::Gen( int32v seed, float32v x, float32v y, float32v z )
{
    float32v xs = FS_Floor_f32( x );
    float32v ys = FS_Floor_f32( y );
    float32v zs = FS_Floor_f32( z );

    int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
    int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
    int32v z0 = FS_Convertf32_i32( zs ) * int32v( Primes::Z );
    int32v x1 = x0 + int32v( Primes::X );
    int32v y1 = y0 + int32v( Primes::Y );
    int32v z1 = z0 + int32v( Primes::Z );

    float32v xf0 = xs = x - xs;
    float32v yf0 = ys = y - ys;
    float32v zf0 = zs = z - zs;
    float32v xf1 = xf0 - float32v( 1 );
    float32v yf1 = yf0 - float32v( 1 );
    float32v zf1 = zf0 - float32v( 1 );

    xs = this->InterpQuintic( xs );
    ys = this->InterpQuintic( ys );
    zs = this->InterpQuintic( zs );

    return this->Lerp( this->Lerp(
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y0, z0 ), xf0, yf0, zf0 ), this->GetGradientDot( this->HashPrimes( seed, x1, y0, z0 ), xf1, yf0, zf0 ), xs),
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y1, z0 ), xf0, yf1, zf0 ), this->GetGradientDot( this->HashPrimes( seed, x1, y1, z0 ), xf1, yf1, zf0 ), xs), ys),
           this->Lerp(
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y0, z1 ), xf0, yf0, zf1 ), this->GetGradientDot( this->HashPrimes( seed, x1, y0, z1 ), xf1, yf0, zf1 ), xs),
           this->Lerp( this->GetGradientDot( this->HashPrimes( seed, x0, y1, z1 ), xf0, yf1, zf1 ), this->GetGradientDot( this->HashPrimes( seed, x1, y1, z1 ), xf1, yf1, zf1 ), xs), ys), zs);
}
