#pragma once

#ifndef FASTNOISE_REGISTER_NODE
#define FASTNOISE_REGISTER_NODE( CLASS ) \
template class FastSIMD::RegisterDispatchClass<FastNoise::CLASS>;\
static_assert( std::is_final_v<FastSIMD::DispatchClass<CLASS, FastSIMD::FeatureSetDefault()>> )
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Generator.h>
#else
#include <FastNoise/Generators/Generator.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/BasicGenerators.h>
#else
#include <FastNoise/Generators/BasicGenerators.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Value.h>
#else
#include <FastNoise/Generators/Value.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Perlin.h>
#else
#include <FastNoise/Generators/Perlin.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Simplex.h>
#else
#include <FastNoise/Generators/Simplex.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Cellular.h>
#else
#include <FastNoise/Generators/Cellular.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Fractal.h>
#else
#include <FastNoise/Generators/Fractal.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/DomainWarp.h>
#else
#include <FastNoise/Generators/DomainWarp.inl>

#endif
#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/DomainWarpSimplex.h>
#else
#include <FastNoise/Generators/DomainWarpSimplex.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/DomainWarpFractal.h>
#else
#include <FastNoise/Generators/DomainWarpFractal.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Modifiers.h>
#else
#include <FastNoise/Generators/Modifiers.inl>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include <FastNoise/Generators/Blends.h>
#else
#include <FastNoise/Generators/Blends.inl>
#endif

// Nodes
// Order is important!
// Always add to bottom of list,
// inserting will break existing encoded node trees

FASTNOISE_REGISTER_NODE( Constant );
FASTNOISE_REGISTER_NODE( White );
FASTNOISE_REGISTER_NODE( Checkerboard );
FASTNOISE_REGISTER_NODE( SineWave );
FASTNOISE_REGISTER_NODE( PositionOutput );
FASTNOISE_REGISTER_NODE( DistanceToPoint );

FASTNOISE_REGISTER_NODE( Simplex );
FASTNOISE_REGISTER_NODE( Perlin );
FASTNOISE_REGISTER_NODE( Value );
                       
FASTNOISE_REGISTER_NODE( CellularValue );
FASTNOISE_REGISTER_NODE( CellularDistance );
FASTNOISE_REGISTER_NODE( CellularLookup );
                       
FASTNOISE_REGISTER_NODE( FractalFBm );
FASTNOISE_REGISTER_NODE( FractalPingPong );
FASTNOISE_REGISTER_NODE( FractalRidged );

FASTNOISE_REGISTER_NODE( DomainWarpSimplex );
FASTNOISE_REGISTER_NODE( DomainWarpGradient );

FASTNOISE_REGISTER_NODE( DomainWarpFractalProgressive );
FASTNOISE_REGISTER_NODE( DomainWarpFractalIndependant );
                       
FASTNOISE_REGISTER_NODE( Add );
FASTNOISE_REGISTER_NODE( Subtract );
FASTNOISE_REGISTER_NODE( Multiply );
FASTNOISE_REGISTER_NODE( Divide );

FASTNOISE_REGISTER_NODE( Abs );
FASTNOISE_REGISTER_NODE( Min );
FASTNOISE_REGISTER_NODE( Max );
FASTNOISE_REGISTER_NODE( MinSmooth );
FASTNOISE_REGISTER_NODE( MaxSmooth );
FASTNOISE_REGISTER_NODE( SquareRoot );
FASTNOISE_REGISTER_NODE( PowFloat );
FASTNOISE_REGISTER_NODE( PowInt );

FASTNOISE_REGISTER_NODE( DomainScale );
FASTNOISE_REGISTER_NODE( DomainOffset );
FASTNOISE_REGISTER_NODE( DomainRotate );
FASTNOISE_REGISTER_NODE( DomainAxisScale );

FASTNOISE_REGISTER_NODE( SeedOffset );
FASTNOISE_REGISTER_NODE( ConvertRGBA8 );
FASTNOISE_REGISTER_NODE( GeneratorCache );

FASTNOISE_REGISTER_NODE( Fade );
FASTNOISE_REGISTER_NODE( Remap );
FASTNOISE_REGISTER_NODE( Terrace );
FASTNOISE_REGISTER_NODE( AddDimension );
FASTNOISE_REGISTER_NODE( RemoveDimension );

FASTNOISE_REGISTER_NODE( Modulus );
