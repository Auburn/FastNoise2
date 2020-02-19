#include "Generator.h"

//#include "../FastSIMD/FS_Class.inl"
//#ifdef FASTSIMD_INCLUDE_CHECK
//#include __FILE__
//#endif
//#include "../FastSIMD/FS_Class.inl"
#pragma once

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
    public:
        static const FastSIMD::Level_BitFlags Supported_SIMD_Levels = FastSIMD::COMPILED_SIMD_LEVELS;


        //FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    /*public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override );*/

    };
}
