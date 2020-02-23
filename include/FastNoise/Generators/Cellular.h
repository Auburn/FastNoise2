#include "Generator.h"

#include "../FastSIMD/FS_Class.inl"
#ifdef FASTSIMD_INCLUDE_CHECK
#include __FILE__
#endif
#include "../FastSIMD/FS_Class.inl"
#pragma once

namespace FastNoise
{
    FASTSIMD_CLASS_DECLARATION_CHILD( Cellular, Generator )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_EXTERNAL(
            enum class DistanceFunction
        {
            Euclidean,
            EuclideanSquared,
            Manhattan,
            Natural,
        } );

        FS_EXTERNAL( void SetJitterModifier( float value ) { mJitterModifier = value; } );
        FS_EXTERNAL( void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; } );

    protected:

        FS_INTERNAL( template<typename... P> FS_INLINE float32v GetDistance( float32v dX, P... d ) );

        FS_EXTERNAL( float mJitterModifier = 1.0f );
        FS_EXTERNAL( DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared );

        FS_EXTERNAL( const float kJitter2D = 0.5f );
        FS_EXTERNAL( const float kJitter3D = 0.45f );
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( CellularValue, Cellular )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override );
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( CellularDistance, Cellular )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override );
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( CellularLookup, Cellular )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_EXTERNAL_FUNC( void SetLookup( const std::shared_ptr<Generator>& gen ) );

        FS_EXTERNAL( void SetLookupFrequency( float freq ) { mLookupFreqX = freq; mLookupFreqY = freq; mLookupFreqZ = freq; mLookupFreqW = freq; } );
        FS_EXTERNAL( void SetLookupFrequencyAxis( float freqX, float freqY = 0.1f, float freqZ = 0.1f, float freqW = 0.1f )
        {
            mLookupFreqX = freqX; mLookupFreqY = freqY; mLookupFreqZ = freqZ; mLookupFreqW = freqW;
        } );

        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override );


    protected:
        FS_EXTERNAL( float mLookupFreqX = 0.1f );
        FS_EXTERNAL( float mLookupFreqY = 0.1f );
        FS_EXTERNAL( float mLookupFreqZ = 0.1f );
        FS_EXTERNAL( float mLookupFreqW = 0.1f );

        FS_EXTERNAL( std::shared_ptr<Generator> lookupBase );
        FS_INTERNAL( FS_CLASS( Generator )<T_FS>* lookup );
    };
}
