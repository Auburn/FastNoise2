#include "FS_Class.inl"
#ifdef FASTSIMD_INCLUDE_CHECK
#include __FILE__
#endif
#include "FS_Class.inl"
#pragma once

#ifdef FS_SIMD_CLASS
#pragma warning( disable:4250 )
#endif

namespace FastNoise
{
    FASTSIMD_CLASS_DECLARATION( Generator )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

        FASTSIMD_DEFINE_DOWNCAST_FUNC( Generator );

        FS_INTERNAL( template<typename F, FastSIMD::ELevel S> friend class FS_CLASS( Generator ) );

    public:
        FS_EXTERNAL( virtual ~Generator() {} );

        FS_EXTERNAL_FUNC( void GenUniformGrid2D( float* noiseOut, float xStart, float yStart, int32_t xSize, int32_t ySize, float xStep, float yStep, int32_t seed ) );
        FS_EXTERNAL_FUNC( void GenUniformGrid3D( float* noiseOut, float xStart, float yStart, float zStart, int32_t xSize, int32_t ySize, int32_t zSize, float xStep, float yStep, float zStep, int32_t seed ) );

        FS_EXTERNAL_FUNC( void GenPositionArray3D( float* noiseOut, float* xPosArray, float* yPosArray, float* zPosArray, int32_t count, float xOffset, float yOffset, float zOffset, int32_t seed ) );

        FS_INTERNAL( virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) = 0 );
        FS_INTERNAL( virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v ) { return Gen( seed, x, y ); } );

    protected:
        FS_INTERNAL( FS_INLINE float32v GetGradientDot( int32v hash, float32v fX, float32v fY ) );
        FS_INTERNAL( FS_INLINE float32v GetGradientDot( int32v hash, float32v fX, float32v fY, float32v fZ ) );
        FS_INTERNAL( template<typename... P> FS_INLINE int32v HashPrimes( int32v seed, P... primedPos ) );
        FS_INTERNAL( template<typename... P> FS_INLINE int32v HashPrimesHB( int32v seed, P... primedPos ) );
        FS_INTERNAL( template<typename... P> FS_INLINE float32v GetValueCoord( int32v seed, P... primedPos ) );

        FS_INTERNAL( FS_INLINE float32v Lerp( float32v a, float32v b, float32v t ) );
        FS_INTERNAL( FS_INLINE float32v InterpQuintic( float32v x ) );

    };


    FASTSIMD_CLASS_DECLARATION_CHILD( Modifier, Generator )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_EXTERNAL_FUNC( void SetSource( const std::shared_ptr<Generator>& gen ) );
        FS_INTERNAL( FS_CLASS( Generator )* GetSourceSIMD() { return mSource; } );

    protected:
        FS_EXTERNAL( std::shared_ptr<Generator> mSourceBase );
        FS_INTERNAL( FS_CLASS( Generator )* mSource );
    };

    FS_INTERNAL(
        namespace Primes
    {
        static const int32_t X = 1619;
        static const int32_t Y = 31337;
        static const int32_t Z = 6971;
        static const int32_t W = 1013;

        static const int32_t Lookup[] = { X,Y,Z,W };
    } )
}
