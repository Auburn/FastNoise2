#pragma once

#ifndef FASTNOISE_REGISTER_NODE
#define FASTNOISE_REGISTER_NODE( CLASS ) \
template class FastSIMD::RegisterDispatchClass<FastNoise::CLASS>
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Generator.h"
#else
#include "Generators/Generator.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/BasicGenerators.h"
#else
#include "Generators/BasicGenerators.inl"
#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Value.h"
//#else
//#include "Generators/Value.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Perlin.h"
//#else
//#include "Generators/Perlin.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Simplex.h"
//#else
//#include "Generators/Simplex.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Cellular.h"
//#else
//#include "Generators/Cellular.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Fractal.h"
//#else
//#include "Generators/Fractal.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/DomainWarp.h"
//#else
//#include "Generators/DomainWarp.inl"
//#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/DomainWarpFractal.h"
//#else
//#include "Generators/DomainWarpFractal.inl"
//#endif
//
#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Modifiers.h"
#else
#include "Generators/Modifiers.inl"
#endif
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Blends.h"
//#else
//#include "Generators/Blends.inl"
//#endif

// Nodes
// Order is important!
// Always add to bottom of list,
// inserting will break existing encoded node trees

FASTNOISE_REGISTER_NODE( Constant );
//FASTNOISE_REGISTER_NODE( White );
//FASTNOISE_REGISTER_NODE( Checkerboard );
FASTNOISE_REGISTER_NODE( SineWave );
//FASTNOISE_REGISTER_NODE( PositionOutput );
//FASTNOISE_REGISTER_NODE( DistanceToPoint );
//                    
//FASTNOISE_REGISTER_NODE( Value );
//FASTNOISE_REGISTER_NODE( Perlin );
//FASTNOISE_REGISTER_NODE( Simplex );
//FASTNOISE_REGISTER_NODE( OpenSimplex2 );
//                    
//FASTNOISE_REGISTER_NODE( CellularValue );
//FASTNOISE_REGISTER_NODE( CellularDistance );
//FASTNOISE_REGISTER_NODE( CellularLookup );
//                    
//FASTNOISE_REGISTER_NODE( FractalFBm );
//FASTNOISE_REGISTER_NODE( FractalPingPong );
//FASTNOISE_REGISTER_NODE( FractalRidged );
//                    
//FASTNOISE_REGISTER_NODE( DomainWarpGradient );
//FASTNOISE_REGISTER_NODE( DomainWarpFractalProgressive );
//FASTNOISE_REGISTER_NODE( DomainWarpFractalIndependant );
//                    
//FASTNOISE_REGISTER_NODE( DomainScale );
//FASTNOISE_REGISTER_NODE( DomainOffset );
//FASTNOISE_REGISTER_NODE( DomainRotate );
//FASTNOISE_REGISTER_NODE( SeedOffset );
//FASTNOISE_REGISTER_NODE( Remap );
FASTNOISE_REGISTER_NODE( ConvertRGBA8 );
//                    
//FASTNOISE_REGISTER_NODE( Add );
//FASTNOISE_REGISTER_NODE( Subtract );
//FASTNOISE_REGISTER_NODE( Multiply );
//FASTNOISE_REGISTER_NODE( Divide );
//FASTNOISE_REGISTER_NODE( Min );
//FASTNOISE_REGISTER_NODE( Max );
//FASTNOISE_REGISTER_NODE( MinSmooth );
//FASTNOISE_REGISTER_NODE( MaxSmooth );
//FASTNOISE_REGISTER_NODE( Fade );
//                    
//FASTNOISE_REGISTER_NODE( Terrace );
//FASTNOISE_REGISTER_NODE( PowFloat );
//FASTNOISE_REGISTER_NODE( PowInt );
//FASTNOISE_REGISTER_NODE( DomainAxisScale );
//FASTNOISE_REGISTER_NODE( AddDimension );
//FASTNOISE_REGISTER_NODE( RemoveDimension );
//FASTNOISE_REGISTER_NODE( GeneratorCache );
