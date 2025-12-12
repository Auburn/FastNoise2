#pragma once
#include "Utility/Config.h"

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
    /// <param name="maxSimdLevel">Max SIMD level, Max = Auto</param>
    /// <returns>SmartNode<T> is guaranteed not nullptr</returns>
    template<typename T>
    SmartNode<T> New( FastSIMD::FeatureSet maxFeatureSet /*= FastSIMD::FeatureSet::Max*/ )
    {
        static_assert( std::is_base_of<Generator, T>::value, "This function should only be used for FastNoise node classes, for example FastNoise::Simplex" );
        static_assert( !std::is_same<decltype(&T::GetMetadata), decltype(&Generator::GetMetadata)>::value, "Cannot create abstract node class, use a derived class, for example: Fractal -> FractalFBm" );

        return SmartNode<T>( FastSIMD::NewDispatchClass<T>( maxFeatureSet, &SmartNodeManager::Allocate ) );
    }

    /// <summary>
    /// Create a tree of FastNoise nodes from an encoded string
    /// </summary>
    /// <example>
    /// FastNoise::SmartNode<> rootNode = FastNoise::NewFromEncodedNodeTree( "DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==" );
    /// </example>
    /// <param name="encodedNodeTreeString">Can be generated using the Node Editor tool</param>
    /// <param name="maxSimdLevel">Max SIMD level, Max = Auto</param>
    /// <returns>Root node of the tree, nullptr for invalid strings</returns>
    FASTNOISE_API SmartNode<> NewFromEncodedNodeTree( const char* encodedNodeTreeString, FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max );
}
