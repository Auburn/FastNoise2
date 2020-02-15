#define FASTSIMD_INTELLISENSE
#include "DomainWarp.h"

template<typename F, FastSIMD::eLevel S>
template<typename... P>
typename FS_CLASS( FastNoise::DomainWarp )<F, S>::float32v
FS_CLASS( FastNoise::DomainWarp )<F, S>::GenT( int32v seed, P... pos )
{
    Warp( seed, float32v( mWarpAmplitude ), (pos * float32v( mWarpFrequency ))..., pos... );

    return this->mSource->Gen( seed, pos... );
}

template<typename F, FastSIMD::eLevel S>
void FS_CLASS( FastNoise::DomainWarpGradient )<F, S>::Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut )
{
    float32v xs = FS_Floor_f32( x );
    float32v ys = FS_Floor_f32( y );

    int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
    int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
    int32v x1 = x0 + int32v( Primes::X );
    int32v y1 = y0 + int32v( Primes::Y );

    xs = this->InterpQuintic( x - xs );
    ys = this->InterpQuintic( y - ys );

#define GRADIENT_COORD( _x, _y )\
    int32v hash##_x##_y = this->HashPrimesHB(seed, x##_x, y##_y );\
    float32v x##_x##_y = FS_Converti32_f32( hash##_x##_y & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );\
    float32v y##_x##_y = FS_Converti32_f32( (hash##_x##_y >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

    GRADIENT_COORD( 0, 0 );
    GRADIENT_COORD( 1, 0 );
    GRADIENT_COORD( 0, 1 );
    GRADIENT_COORD( 1, 1 );

#undef GRADIENT_COORD

    xOut = FS_FMulAdd_f32( this->Lerp( this->Lerp( x00, x10, xs ), this->Lerp( x01, x11, xs ), ys ), warpAmp * float32v( 1.0f / (0xffff / 2.0f) ), xOut );
    yOut = FS_FMulAdd_f32( this->Lerp( this->Lerp( y00, y10, xs ), this->Lerp( y01, y11, xs ), ys ), warpAmp * float32v( 1.0f / (0xffff / 2.0f) ), yOut );
}

template<typename F, FastSIMD::eLevel S>
void FS_CLASS( FastNoise::DomainWarpGradient )<F, S>::Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut )
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

#define GRADIENT_COORD( _x, _y, _z )\
    int32v hash##_x##_y##_z = this->HashPrimesHB( seed, x##_x, y##_y, z##_z );\
    float32v x##_x##_y##_z = FS_Converti32_f32( hash##_x##_y##_z & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );\
    float32v y##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );\
    float32v z##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

    GRADIENT_COORD( 0, 0, 0 );
    GRADIENT_COORD( 1, 0, 0 );
    GRADIENT_COORD( 0, 1, 0 );
    GRADIENT_COORD( 1, 1, 0 );
    GRADIENT_COORD( 0, 0, 1 );
    GRADIENT_COORD( 1, 0, 1 );
    GRADIENT_COORD( 0, 1, 1 );
    GRADIENT_COORD( 1, 1, 1 );

#undef GRADIENT_COORD

    float32v x0y = this->Lerp( this->Lerp( x000, x100, xs ), this->Lerp( x010, x110, xs ), ys );
    float32v y0y = this->Lerp( this->Lerp( y000, y100, xs ), this->Lerp( y010, y110, xs ), ys );
    float32v z0y = this->Lerp( this->Lerp( z000, z100, xs ), this->Lerp( z010, z110, xs ), ys );

    float32v x1y = this->Lerp( this->Lerp( x001, x101, xs ), this->Lerp( x011, x111, xs ), ys );
    float32v y1y = this->Lerp( this->Lerp( y001, y101, xs ), this->Lerp( y011, y111, xs ), ys );
    float32v z1y = this->Lerp( this->Lerp( z001, z101, xs ), this->Lerp( z011, z111, xs ), ys );

    xOut = FS_FMulAdd_f32( this->Lerp( x0y, x1y, zs ), warpAmp * float32v( 1.0f / (0x3ff / 2.0f) ), xOut );
    yOut = FS_FMulAdd_f32( this->Lerp( y0y, y1y, zs ), warpAmp * float32v( 1.0f / (0x3ff / 2.0f) ), yOut );
    zOut = FS_FMulAdd_f32( this->Lerp( z0y, z1y, zs ), warpAmp * float32v( 1.0f / (0x3ff / 2.0f) ), zOut );
}