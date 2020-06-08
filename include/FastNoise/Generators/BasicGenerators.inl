#include <cassert>
#include "FastSIMD/InlInclude.h"

#include "BasicGenerators.h"

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
public:
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<typename FS>
class FS_T<FastNoise::PositionOutput, FS> : public virtual FastNoise::PositionOutput, public FS_T<FastNoise::Generator, FS>
{
public:
    //FASTNOISE_IMPL_GEN_T;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        float32v out = (x + float32v( mOffsetX )) * float32v( mMultiplierX );
        out += (y + float32v( mOffsetY )) * float32v( mMultiplierY );

        return out;
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        float32v out = (x + float32v( mOffsetX )) * float32v( mMultiplierX );
        out += (y + float32v( mOffsetY )) * float32v( mMultiplierY );
        out += (y + float32v( mOffsetZ )) * float32v( mMultiplierZ );

        return out;
    }
};
