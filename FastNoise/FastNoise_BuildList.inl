#pragma once

#ifndef FASTSIMD_BUILD_CLASS
#error Do not include this file
#endif

#ifndef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Generator.inl"
#endif

//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/White.h"
//#else
//#include "Generators/White.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::White )
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Value.h"
//#else
//#include "Generators/Value.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::Value )
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Perlin.h"
//#else
//#include "Generators/Perlin.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::Perlin )

#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Simplex.h"
#else
#include "Generators/Simplex.inl"
#endif
FASTSIMD_BUILD_CLASS( FastNoise::Simplex )

//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Cellular.h"
//#else
//#include "Generators/Cellular.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::CellularValue )
//FASTSIMD_BUILD_CLASS( FastNoise::CellularDistance )
//FASTSIMD_BUILD_CLASS( FastNoise::CellularLookup )
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/Fractal.h"
//#else
//#include "Generators/Fractal.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::FractalFBm )
//FASTSIMD_BUILD_CLASS( FastNoise::FractalBillow )
//FASTSIMD_BUILD_CLASS( FastNoise::FractalRidged )
//FASTSIMD_BUILD_CLASS( FastNoise::FractalRidgedMulti )
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/DomainWarp.h"
//#else
//#include "Generators/DomainWarp.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpGradient )
//
//#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
//#include "Generators/DomainWarpFractal.h"
//#else
//#include "Generators/DomainWarpFractal.inl"
//#endif
//FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpFractalProgressive )
//FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpFractalIndependant )
//
//
#ifdef FASTSIMD_INCLUDE_HEADER_ONLY
#include "Generators/Modifiers.h"
#else
#include "Generators/Modifiers.inl"
#endif
FASTSIMD_BUILD_CLASS( FastNoise::DomainScale )
//FASTSIMD_BUILD_CLASS( FastNoise::Remap )

