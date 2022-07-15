#pragma once
#include "FastNoise_Export.h"
#include <FastSIMD/DispatchClass.h>

#define FASTNOISE_CALC_MIN_MAX true
#define FASTNOISE_USE_SHARED_PTR false

#if FASTNOISE_USE_SHARED_PTR
#include <memory>
#endif

namespace FastNoise
{    
    class Generator;
    struct Metadata;

    template<typename T>
    struct MetadataT;

#if FASTNOISE_USE_SHARED_PTR
    template<typename T = Generator>
    using SmartNode = std::shared_ptr<T>;
#else
    template<typename T = Generator>
    class SmartNode;
#endif

    template<typename T = Generator>
    using SmartNodeArg = const SmartNode<const T>&;

    template<typename T>
    SmartNode<T> New( FastSIMD::FeatureSet maxFeatureSet = FastSIMD::FeatureSet::Max );
} // namespace FastNoise

#if !FASTNOISE_USE_SHARED_PTR
#include "SmartNode.h"
#endif