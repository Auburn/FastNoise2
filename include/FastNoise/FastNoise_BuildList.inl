#pragma once

#ifndef FASTSIMD_BUILD_CLASS
#error Do not include this file
#endif

namespace FastNoise {}
using namespace FastNoise;

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

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Value.h"
#else
#include "Generators/Value.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Perlin.h"
#else
#include "Generators/Perlin.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Simplex.h"
#else
#include "Generators/Simplex.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Cellular.h"
#else
#include "Generators/Cellular.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Fractal.h"
#else
#include "Generators/Fractal.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/DomainWarp.h"
#else
#include "Generators/DomainWarp.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/DomainWarpFractal.h"
#else
#include "Generators/DomainWarpFractal.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Modifiers.h"
#else
#include "Generators/Modifiers.inl"
#endif

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Blends.h"
#else
#include "Generators/Blends.inl"
#endif

// Nodes
// Order is important!
// Always add to bottom of list,
// inserting will break existing encoded node trees

FASTSIMD_BUILD_CLASS( Constant )
FASTSIMD_BUILD_CLASS( White )
FASTSIMD_BUILD_CLASS( Checkerboard )
FASTSIMD_BUILD_CLASS( SineWave )
FASTSIMD_BUILD_CLASS( PositionOutput )
FASTSIMD_BUILD_CLASS( DistanceToOrigin )

FASTSIMD_BUILD_CLASS( Value )
FASTSIMD_BUILD_CLASS( Perlin )
FASTSIMD_BUILD_CLASS( Simplex )
FASTSIMD_BUILD_CLASS( OpenSimplex2 )

FASTSIMD_BUILD_CLASS( CellularValue )
FASTSIMD_BUILD_CLASS( CellularDistance )
FASTSIMD_BUILD_CLASS( CellularLookup )

FASTSIMD_BUILD_CLASS( FractalFBm )
FASTSIMD_BUILD_CLASS( FractalBillow )
FASTSIMD_BUILD_CLASS( FractalRidged )
FASTSIMD_BUILD_CLASS( FractalRidgedMulti )

FASTSIMD_BUILD_CLASS( DomainWarpGradient )
FASTSIMD_BUILD_CLASS( DomainWarpFractalProgressive )
FASTSIMD_BUILD_CLASS( DomainWarpFractalIndependant )

FASTSIMD_BUILD_CLASS( DomainScale )
FASTSIMD_BUILD_CLASS( DomainOffset )
FASTSIMD_BUILD_CLASS( DomainRotate )
FASTSIMD_BUILD_CLASS( SeedOffset )
FASTSIMD_BUILD_CLASS( Remap )
FASTSIMD_BUILD_CLASS( ConvertRGBA8 )

FASTSIMD_BUILD_CLASS( Add )
FASTSIMD_BUILD_CLASS( Subtract )
FASTSIMD_BUILD_CLASS( Multiply )
FASTSIMD_BUILD_CLASS( Divide )
FASTSIMD_BUILD_CLASS( Min )
FASTSIMD_BUILD_CLASS( Max )
FASTSIMD_BUILD_CLASS( MinSmooth )
FASTSIMD_BUILD_CLASS( MaxSmooth )
FASTSIMD_BUILD_CLASS( Fade )

FASTSIMD_BUILD_CLASS( Terrace )
