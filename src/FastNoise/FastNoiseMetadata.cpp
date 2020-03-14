#include "FastNoise/FastNoiseMetadata.h"

std::vector<const FastNoise::Metadata*> FastNoise::MetadataManager::sMetadataClasses;

#define FASTSIMD_BUILD_CLASS( CLASS ) \
CLASS::Metadata g ## CLASS ## Metadata( #CLASS );\
FastNoise::Metadata* CLASS::GetMetadata()\
{\
    return &g ## CLASS ## Metadata;\
}

#define FASTSIMD_INCLUDE_HEADER_ONLY
#include "FastNoise/FastNoise_BuildList.inl"