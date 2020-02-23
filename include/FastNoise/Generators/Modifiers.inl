#include "Modifiers.h"

#include "FastSIMD/FunctionList.h"

namespace FastNoise
{
    template<typename FS>
    class DomainScale_FS : public virtual DomainScale, public Modifier_FS<FS, 1>
    {
        FASTSIMD_TYPEDEF;

    public:
        FASTNOISE_IMPL_GEN_T;
        
        template<typename... P> 
        float32v FS_INLINE GenT( int32v seed, P... pos )
        {
            return this->mSource[0]->Gen( seed, (pos * float32v( mScale ))... );
        }
    };

    template<typename FS>
    class Remap_FS : public virtual Remap, public Modifier_FS<FS, 1>
    {
        FASTSIMD_TYPEDEF;

    public:
        FASTNOISE_IMPL_GEN_T;

        template<typename... P>
        float32v FS_INLINE GenT(int32v seed, P... pos)
        {
            float32v source = this->mSource[0]->Gen(seed, pos...);
                
            return float32v( mToMin ) + FS_Min_f32( float32v( 1.0f ), FS_Max_f32( float32v( 0.0f ), ((source - float32v( mFromMin )) / float32v( mFromMax - mFromMin )) * float32v( mToMax - mToMin ) ) );
        }
    };
}
