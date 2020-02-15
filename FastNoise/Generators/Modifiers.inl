#define FASTSIMD_INTELLISENSE
#include "Modifiers.h"

template<typename F, FastSIMD::eLevel S>
template<typename... P>
typename FS_CLASS( FastNoise::DomainScale )<F, S>::float32v
FS_CLASS( FastNoise::DomainScale )<F, S>::GenT( int32v seed, P... pos )
{
    return FS_CLASS( Modifier )<FS>::mSource->Gen( seed, (pos * float32v( mScale ))... );
}

template<typename F, FastSIMD::eLevel S>
template<typename... P>
typename FS_CLASS( FastNoise::Remap )<F, S>::float32v
FS_CLASS( FastNoise::Remap )<F, S>::GenT( int32v seed, P... pos )
{
    float32v source = FS_CLASS( Modifier )<FS>::mSource->Gen( seed, pos... );
    
    return float32v( mToMin ) + FS_Min_f32( float32v( 1.0f ), FS_Max_f32( float32v( 0.0f ), ((source - float32v( mFromMin )) / float32v( mFromMax - mFromMin )) * float32v( mToMax - mToMin ) ) );
}
