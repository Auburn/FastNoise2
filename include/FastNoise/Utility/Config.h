#pragma once
#include "Export.h"
#include <FastSIMD/DispatchClass.h>

#define FASTNOISE_CALC_MIN_MAX true

namespace FastNoise
{    
    class Generator;
    struct Metadata;

    template<typename T>
    struct MetadataT;

    template<typename T = Generator>
    class SmartNode;

    /** @brief Alias for passing SmartNode references as function arguments.
     *
     *  Used throughout the API for setter functions that accept a source node
     *  (e.g. `SetSource( SmartNodeArg<> gen )`). Equivalent to `const SmartNode<const T>&`.
     *
     *  @tparam T  Node type constraint. Defaults to Generator (accepts any node).
     */
    template<typename T = Generator>
    using SmartNodeArg = const SmartNode<const T>&;

    template<typename T>
    SmartNode<T> New( FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max );
} // namespace FastNoise

#include "SmartNode.h"