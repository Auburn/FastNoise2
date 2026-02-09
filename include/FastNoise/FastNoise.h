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
#include "Generators/DomainWarpSimplex.h"
#include "Generators/DomainWarpFractal.h"
#include "Generators/Modifiers.h"
#include "Generators/Blends.h"

namespace FastNoise
{
    /** @brief Create a new instance of a FastNoise generator node.
     *
     *  This is the primary way to create nodes in code. The returned SmartNode
     *  manages the node's lifetime via reference counting.
     *
     *  @code
     *  auto simplex = FastNoise::New<FastNoise::Simplex>();
     *  auto fractal = FastNoise::New<FastNoise::FractalFBm>();
     *  fractal->SetSource( simplex );
     *  @endcode
     *
     *  @tparam T  Concrete node class to create (e.g. FastNoise::Simplex, FastNoise::FractalFBm).
     *             Must not be an abstract base class.
     *  @param  maxFeatureSet  Maximum SIMD feature set to use. Defaults to auto-detecting
     *                         the best available on the current CPU.
     *  @return A SmartNode<T> owning the new node. null if the maxFeatureSet is below the min compiled SIMD feature set
     */
    template<typename T>
    SmartNode<T> New( FastSIMD::FeatureSet maxFeatureSet /*= FastSIMD::FeatureSet::Max*/ )
    {
        static_assert( std::is_base_of<Generator, T>::value, "This function should only be used for FastNoise node classes, for example FastNoise::Simplex" );
        static_assert( !std::is_same<decltype(&T::GetMetadata), decltype(&Generator::GetMetadata)>::value, "Cannot create abstract node class, use a derived class, for example: Fractal -> FractalFBm" );

        return SmartNode<T>( FastSIMD::NewDispatchClass<T>( maxFeatureSet, &SmartNodeManager::Allocate ) );
    }

    /** @brief Create a tree of FastNoise nodes from an encoded string.
     *
     *  Deserialises a node tree that was previously serialised to a string.
     *  Encoded strings can be generated using the Node Editor tool (right-click a node
     *  title and select "Copy Encoded Node Tree"), or programmatically via
     *  Metadata::SerialiseNodeData().
     *
     *  @code
     *  FastNoise::SmartNode<> generator = FastNoise::NewFromEncodedNodeTree( "DQkGDA==" );
     *  if( generator )
     *  {
     *      std::vector<float> noise( 256 * 256 );
     *      generator->GenUniformGrid2D( noise.data(), 0, 0, 256, 256, 1, 1, 1337 );
     *  }
     *  @endcode
     *
     *  @param  encodedNodeTreeString  Encoded node tree string.
     *  @param  maxFeatureSet          Maximum SIMD feature set to use. Defaults to auto-detect.
     *  @return Root node of the deserialised tree, or nullptr if the string is invalid.
     */
    FASTNOISE_API SmartNode<> NewFromEncodedNodeTree( const char* encodedNodeTreeString, FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max );
}
