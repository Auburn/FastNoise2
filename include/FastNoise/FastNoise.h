#pragma once
#include "FastNoise_Config.h"

// Node class definitions
#include "Generators/BasicGenerators.h"
#include "Generators/Value.h"
#include "Generators/Perlin.h"
#include "Generators/Simplex.h"
#include "Generators/Cellular.h"
#include "Generators/Fractal.h"
#include "Generators/DomainWarp.h"
#include "Generators/DomainWarpFractal.h"
#include "Generators/Modifiers.h"
#include "Generators/Blends.h"

namespace FastNoise
{
    /// <summary>
    /// Create new instance of a FastNoise node
    /// </summary>
    /// <example>
    /// auto node = FastNoise::New<FastNoise::Simplex>();
    /// </example>
    /// <typeparam name="T">Node class to create</typeparam>
    /// <param name="maxSimdLevel">Max SIMD level, Null = Auto</param>
    /// <returns>SmartNode<T> is guaranteed not nullptr</returns>
    template<typename T>
    SmartNode<T> New( FastSIMD::eLevel maxSimdLevel = FastSIMD::Level_Null )
    {
        static_assert( std::is_base_of<Generator, T>::value, "Use FastSIMD::New() to create non FastNoise classes" );
        static_assert( std::is_member_function_pointer<decltype(&T::GetMetadata)>::value, "Cannot create abstract node class, use a derived class, for example: Fractal -> FractalFBm" );

        return SmartNode<T>( FastSIMD::New<T>( maxSimdLevel ) );
    }

    /// <summary>
    /// Create a tree of FastNoise nodes from an encoded string
    /// </summary>
    /// <example>
    /// FastNoise::SmartNode<> rootNode = FastNoise::NewFromEncodedNodeTree( "DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==" );
    /// </example>
    /// <param name="encodedNodeTreeString">Can be generated using the NoiseTool</param>
    /// <param name="maxSimdLevel">Max SIMD level, Null = Auto</param>
    /// <returns>Root node of the tree, nullptr for invalid strings</returns>
    SmartNode<> NewFromEncodedNodeTree( const char* encodedNodeTreeString, FastSIMD::eLevel maxSimdLevel = FastSIMD::Level_Null );
}