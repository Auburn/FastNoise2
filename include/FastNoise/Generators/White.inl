#include "FastSIMD/InlInclude.h"

#include "White.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::White, FS> : public virtual FastNoise::White, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos = FS_Casti32_f32( (FS_Castf32_i32( pos ) ^ (FS_Castf32_i32( pos ) >> 16)) * int32v( Primes::Lookup[idx++] ) )), ...);

        return GetValueCoord( seed, FS_Castf32_i32( pos )... );
    }
};
