#define FASTSIMD_INTELLISENSE
#include "Value.h"

template<typename F, FastSIMD::eLevel S>
typename FS_CLASS( FastNoise::Value )<F, S>::float32v
FS_CLASS( FastNoise::Value )<F, S>::Gen( int32v seed, float32v x, float32v y )
{
    float32v xs = FS_Floor_f32( x );
    float32v ys = FS_Floor_f32( y );

    int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
    int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
    int32v x1 = x0 + int32v( Primes::X );
    int32v y1 = y0 + int32v( Primes::Y );

    xs = this->InterpQuintic( x - xs );
    ys = this->InterpQuintic( y - ys );

    return this->Lerp(
           this->Lerp( this->GetValueCoord( seed, x0, y0 ), this->GetValueCoord( seed, x1, y0 ), xs),
           this->Lerp( this->GetValueCoord( seed, x0, y1 ), this->GetValueCoord( seed, x1, y1 ), xs), ys);
}

template<typename F, FastSIMD::eLevel S>
typename FS_CLASS( FastNoise::Value )<F, S>::float32v
FS_CLASS( FastNoise::Value )<F, S>::Gen( int32v seed, float32v x, float32v y, float32v z )
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

    xs = this->InterpQuintic( x - xs );
    ys = this->InterpQuintic( y - ys );
    zs = this->InterpQuintic( z - zs );

    return this->Lerp( this->Lerp(
           this->Lerp( this->GetValueCoord( seed, x0, y0, z0 ), this->GetValueCoord( seed, x1, y0, z0 ), xs),
           this->Lerp( this->GetValueCoord( seed, x0, y1, z0 ), this->GetValueCoord( seed, x1, y1, z0 ), xs), ys),
           this->Lerp( 
           this->Lerp( this->GetValueCoord( seed, x0, y0, z1 ), this->GetValueCoord( seed, x1, y0, z1 ), xs),
           this->Lerp( this->GetValueCoord( seed, x0, y1, z1 ), this->GetValueCoord( seed, x1, y1, z1 ), xs), ys), zs);
}
