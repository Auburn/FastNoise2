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

    template<typename T = Generator>
    using SmartNodeArg = const SmartNode<const T>&;

    template<typename T>
    SmartNode<T> New( FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max );
} // namespace FastNoise

#include "SmartNode.h"