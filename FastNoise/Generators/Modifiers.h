#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainScale : public virtual Modifier<1>
    {
    public:
        static const FastSIMD::Level_BitFlags Supported_SIMD_Levels = FastSIMD::COMPILED_SIMD_LEVELS;

        void SetScale( float value ) { mScale = value; };

    protected:
        float mScale = 1.0f;
    };

   /* FASTSIMD_CLASS_DECLARATION_CHILD( Remap, Modifier )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_EXTERNAL( void SetRemap( float fromMin, float fromMax, float toMin, float toMax ) { mFromMin = fromMin; mFromMax = fromMax; mToMin = toMin; mToMax = toMax; } );

        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override { return GenT( seed, x, y ); } );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override { return GenT( seed, x, y, z ); } );

    protected:
        FS_EXTERNAL( float mFromMin = 0.0f );
        FS_EXTERNAL( float mFromMax = 0.0f );
        FS_EXTERNAL( float mToMin = 0.0f );
        FS_EXTERNAL( float mToMax = 0.0f );

    private:
        FS_INTERNAL( template<typename... P> float32v FS_INLINE GenT( int32v seed, P... pos ); )

    };*/
}
