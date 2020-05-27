#include "FastSIMD/InlInclude.h"

#include "White.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::White, FS> : public virtual FastNoise::White, public FS_T<FastNoise::Generator, FS>
{
public:
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        return GetValueCoord( seed, 
            (FS_Castf32_i32( x ) ^ (FS_Castf32_i32( x ) >> 16)) * int32v( Primes::X ),
            (FS_Castf32_i32( y ) ^ (FS_Castf32_i32( y ) >> 16)) * int32v( Primes::Y ));
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        return GetValueCoord( seed, 
            (FS_Castf32_i32( x ) ^ (FS_Castf32_i32( x ) >> 16)) * int32v( Primes::X ),
            (FS_Castf32_i32( y ) ^ (FS_Castf32_i32( y ) >> 16)) * int32v( Primes::Y ),
            (FS_Castf32_i32( z ) ^ (FS_Castf32_i32( z ) >> 16)) * int32v( Primes::Z ));
    }
};
