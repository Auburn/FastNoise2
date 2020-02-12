#pragma once

#ifndef FASTSIMD_BUILD_CLASS
#error Do not include this file
#endif

#include "Example/Example.inl"
FASTSIMD_BUILD_CLASS( Example )

#include "../FastNoise/Generators/Generator.inl"

#include "../FastNoise/Generators/White.inl"
FASTSIMD_BUILD_CLASS( FastNoise::White )

#include "../FastNoise/Generators/Value.inl"
FASTSIMD_BUILD_CLASS( FastNoise::Value )

#include "../FastNoise/Generators/Perlin.inl"
FASTSIMD_BUILD_CLASS( FastNoise::Perlin )

#include "../FastNoise/Generators/Simplex.inl"
FASTSIMD_BUILD_CLASS( FastNoise::Simplex )

#include "../FastNoise/Generators/Cellular.inl"
FASTSIMD_BUILD_CLASS( FastNoise::CellularValue )
FASTSIMD_BUILD_CLASS( FastNoise::CellularDistance )
FASTSIMD_BUILD_CLASS( FastNoise::CellularLookup )

#include "../FastNoise/Generators/Fractal.inl"
FASTSIMD_BUILD_CLASS( FastNoise::FractalFBm )
FASTSIMD_BUILD_CLASS( FastNoise::FractalBillow )
FASTSIMD_BUILD_CLASS( FastNoise::FractalRidged )
FASTSIMD_BUILD_CLASS( FastNoise::FractalRidgedMulti )

#include "../FastNoise/Generators/DomainWarp.inl"
FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpGradient )

#include "../FastNoise/Generators/DomainWarpFractal.inl"
FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpFractalProgressive )
FASTSIMD_BUILD_CLASS( FastNoise::DomainWarpFractalIndependant )
