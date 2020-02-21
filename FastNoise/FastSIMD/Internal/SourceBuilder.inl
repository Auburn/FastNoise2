#pragma once
#include "../FastSIMD.h"
#include "../TypeList.h"

#define FASTSIMD_BUILD_CLASS( CLASS ) \
template<FastSIMD::eLevel SIMD_LEVEL>\
struct FastSIMD::ClassFactory<FS_ENABLE_IF( (CLASS::Supported_SIMD_Levels & SIMD_LEVEL & FastSIMD::COMPILED_SIMD_LEVELS) == 0, CLASS ), SIMD_LEVEL>\
{\
static CLASS* Get()\
{\
return nullptr; \
}\
};\
template<FastSIMD::eLevel SIMD_LEVEL>\
struct FastSIMD::ClassFactory<FS_ENABLE_IF( (CLASS::Supported_SIMD_Levels & SIMD_LEVEL & FastSIMD::COMPILED_SIMD_LEVELS) != 0, CLASS ), SIMD_LEVEL>\
{\
static CLASS* Get()\
{\
return new FS_CLASS( CLASS )<FS_SIMD_CLASS>; \
}\
};\
template struct FastSIMD::ClassFactory<CLASS, FS_SIMD_CLASS::SIMD_Level>;

#include "../FastSIMD_BuildList.inl"
