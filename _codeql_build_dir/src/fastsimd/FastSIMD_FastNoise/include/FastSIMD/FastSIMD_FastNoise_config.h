#pragma once

#include <FastSIMD/Utility/ArchDetect.h>
#include <FastSIMD/Utility/FeatureSetList.h>

namespace FastSIMD
{
namespace FastSIMD_FastNoise
{
using CompiledFeatureSets = FeatureSetList<0
,FastSIMD::FeatureSet::SSE2
,FastSIMD::FeatureSet::SSE41
,FastSIMD::FeatureSet::AVX2
,FastSIMD::FeatureSet::AVX512
>;
}
}
