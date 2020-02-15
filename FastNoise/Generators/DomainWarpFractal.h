#include "DomainWarp.h"

#include "../FastSIMD/FS_Class.inl"
#ifdef FASTSIMD_INCLUDE_CHECK
#include __FILE__
#endif
#include "../FastSIMD/FS_Class.inl"
#pragma once

namespace FastNoise
{
    FASTSIMD_CLASS_DECLARATION_CHILD( DomainWarpFractal, Generator )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_EXTERNAL_FUNC( void SetDomainWarpSource( const std::shared_ptr<DomainWarp>& domainWarp ) );

        FS_EXTERNAL( void SetLacunarity( float value ) { mLacunarity = value; } );
        FS_EXTERNAL( void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } );
        FS_EXTERNAL( void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } );

    protected:
        FS_EXTERNAL( std::shared_ptr<DomainWarp> mWarpBase );
        FS_INTERNAL( FS_CLASS( DomainWarp )<FS>* mWarp );

        FS_EXTERNAL( float mLacunarity = 2.0f );
        FS_EXTERNAL( float mGain = 0.5f );
        FS_EXTERNAL( float mFractalBounding = 1.0f / 1.75f );
        FS_EXTERNAL( int32_t mOctaves = 3 );

    private:

        FS_EXTERNAL(
            void CalculateFractalBounding()
        {
            float amp = mGain;
            float ampFractal = 1.0f;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                ampFractal += amp;
                amp *= mGain;
            }
            mFractalBounding = 1.0f / ampFractal;
        } );
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( DomainWarpFractalProgressive, DomainWarpFractal )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override { return GenT( seed, x, y ); } );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override { return GenT( seed, x, y, z ); } );

    private:
        FS_INTERNAL( template<typename... P> float32v FS_INLINE GenT( int32v seed, P... pos ); )
    };

    FASTSIMD_CLASS_DECLARATION_CHILD( DomainWarpFractalIndependant, DomainWarpFractal )
    {
        FASTSIMD_CLASS_SETUP( FastSIMD::COMPILED_SIMD_LEVELS );

    public:
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) override { return GenT( seed, x, y ); } );
        FS_INTERNAL( float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) override { return GenT( seed, x, y, z ); } );

    private:
        FS_INTERNAL( template<typename... P, std::size_t... I> float32v FS_INLINE GenT( int32v seed, std::index_sequence<I...>, P... pos ); )
        FS_INTERNAL( template<typename... P> float32v FS_INLINE GenT( int32v seed, P... pos ) { return GenT( seed, std::index_sequence_for<P...>(), pos... ); } )
    };
}
