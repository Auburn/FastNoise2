#define FASTSIMD_INTELLISENSE
#include "White.h"

template<typename F, FastSIMD::eLevel S>
template<typename... P>
typename FS_CLASS( FastNoise::White )<F, S>::float32v
FS_CLASS( FastNoise::White )<F, S>::GenT( int32v seed, P... pos )
{
    int idx = 0;

    std::initializer_list<float32v>{ (pos = FS_Casti32_f32( (FS_Castf32_i32( pos ) ^ (FS_Castf32_i32( pos ) >> 16)) * int32v( Primes::Lookup[idx++] )))... };

    return this->GetValueCoord( seed, FS_Castf32_i32( pos )... );
}