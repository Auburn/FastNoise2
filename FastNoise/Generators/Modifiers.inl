#define FASTSIMD_INTELLISENSE
#include "Modifiers.h"

#include "../FastSIMD/Internal/FunctionList.h"

namespace FastNoise
{
    template<typename FS>
    class DomainScale_FS : public virtual DomainScale, public Modifier_FS<FS, 1>
    {
        typedef typename FS::float32v float32v;
        typedef typename FS::int32v int32v;
        typedef typename FS::mask32v mask32v;

    public:
        virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) final { return GenT( seed, x, y ); }
        virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) final { return GenT( seed, x, y, z ); }
        
        template<typename... P> 
        float32v FS_INLINE GenT( int32v seed, P... pos )
        {
            return mSource[0]->Gen( seed, (pos * float32v( mScale ))... );
        }
    };

}

//template<typename F, FastSIMD::eLevel S>
//template<typename... P>
//typename FS_CLASS( FastNoise::DomainScale )<F, S>::float32v
//FS_CLASS( FastNoise::DomainScale )<F, S>::GenT( int32v seed, P... pos )
//{
//    return FS_CLASS( Modifier )<FS>::mSource->Gen( seed, (pos * float32v( mScale ))... );
//}
//
//template<typename F, FastSIMD::eLevel S>
//template<typename... P>
//typename FS_CLASS( FastNoise::Remap )<F, S>::float32v
//FS_CLASS( FastNoise::Remap )<F, S>::GenT( int32v seed, P... pos )
//{
//    float32v source = FS_CLASS( Modifier )<FS>::mSource->Gen( seed, pos... );
//    
//    return float32v( mToMin ) + FS_Min_f32( float32v( 1.0f ), FS_Max_f32( float32v( 0.0f ), ((source - float32v( mFromMin )) / float32v( mFromMax - mFromMin )) * float32v( mToMax - mToMin ) ) );
//}
