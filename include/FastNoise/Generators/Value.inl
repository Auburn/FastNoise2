#include "FastSIMD/InlInclude.h"

#include "Value.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::Value, FS> : public virtual FastNoise::Value, public FS_T<FastNoise::Generator, FS>
{
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        xs = InterpQuintic( x - xs );
        ys = InterpQuintic( y - ys );

        return Lerp(
            Lerp( GetValueCoord( seed, x0, y0 ), GetValueCoord( seed, x1, y0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1 ), GetValueCoord( seed, x1, y1 ), xs ), ys );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );

        int32v x0 = FS_Convertf32_i32(xs) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32(ys) * int32v( Primes::Y );
        int32v z0 = FS_Convertf32_i32(zs) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        xs = InterpQuintic( x - xs );
        ys = InterpQuintic( y - ys );
        zs = InterpQuintic( z - zs );

        return Lerp( Lerp(
            Lerp( GetValueCoord( seed, x0, y0, z0 ), GetValueCoord( seed, x1, y0, z0 ), xs ),
            Lerp( GetValueCoord( seed, x0, y1, z0 ), GetValueCoord( seed, x1, y1, z0 ), xs ), ys ),
            Lerp(                                                                                
            Lerp( GetValueCoord( seed, x0, y0, z1 ), GetValueCoord( seed, x1, y0, z1 ), xs ),    
            Lerp( GetValueCoord( seed, x0, y1, z1 ), GetValueCoord( seed, x1, y1, z1 ), xs ), ys ), zs );
    }
};
