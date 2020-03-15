#include "FastNoise/FastNoiseMetadata.h"

std::vector<const FastNoise::Metadata*> FastNoise::MetadataManager::sMetadataClasses;

#define FASTSIMD_BUILD_CLASS( CLASS ) \
const CLASS::Metadata g ## CLASS ## Metadata( #CLASS );\
const FastNoise::Metadata* CLASS::GetMetadata()\
{\
    return &g ## CLASS ## Metadata;\
}\
Generator* CLASS::Metadata::NodeFactory( FastSIMD::eLevel l ) const\
{\
    return FastSIMD::New<CLASS>( l );\
}

#define FASTSIMD_INCLUDE_HEADER_ONLY
#include "FastNoise/FastNoise_BuildList.inl"