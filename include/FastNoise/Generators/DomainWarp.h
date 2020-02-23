#include "Generator.h"

#include "../FastSIMD/FS_Class.inl"
#ifdef FASTSIMD_INCLUDE_CHECK
#include __FILE__
#endif
#include "../FastSIMD/FS_Class.inl"
#pragma once

namespace FastNoise
{
    FASTSIMD_CLASS_DECLARATION_CHILD( DomainWarp, Modifier )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

        FASTSIMD_DEFINE_DOWNCAST_FUNC( DomainWarp );

    public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override { return GenT( seed, x, y ); } );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override { return GenT( seed, x, y, z ); } );

        FS_INTERNAL( virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) = 0 );
        FS_INTERNAL( virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) = 0 );

        FS_EXTERNAL( float GetWarpFrequency() { return mWarpFrequency; } )
            FS_EXTERNAL( void SetWarpFrequency( float value ) { mWarpFrequency = value; } )

            FS_EXTERNAL( float GetWarpAmplitude() { return mWarpAmplitude; } )
            FS_EXTERNAL( void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } )

    protected:
        FS_EXTERNAL( float mWarpFrequency = 0.5f );
        FS_EXTERNAL( float mWarpAmplitude = 1.0f );

    private:
        FS_INTERNAL( template<typename... P> float32v FS_INLINE GenT( int32v seed, P... pos ); )
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( DomainWarpGradient, DomainWarp )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_INTERNAL( void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) override );
        FS_INTERNAL( void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) override );

    };
}
