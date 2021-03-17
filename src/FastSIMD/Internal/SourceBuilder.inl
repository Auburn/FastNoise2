#pragma once
#include "FastSIMD/FastSIMD.h"

template<typename CLASS, typename FS>
class FS_T;

template<typename CLASS, FastSIMD::eLevel LEVEL>
CLASS* FastSIMD::ClassFactory( FastSIMD::MemoryResource memoryResource ) 
{
    if constexpr( ( CLASS::Supported_SIMD_Levels & LEVEL & FastSIMD::COMPILED_SIMD_LEVELS ) != 0 )
    {
        static_assert( std::is_base_of_v<CLASS, FS_T<CLASS, FS_SIMD_CLASS>> );

        if( memoryResource )
        {
            void* alloc = memoryResource->allocate( sizeof( FS_T<CLASS, FS_SIMD_CLASS> ), alignof( FS_T<CLASS, FS_SIMD_CLASS> ) );
            
            return new( alloc ) FS_T<CLASS, FS_SIMD_CLASS>;
        }

        return new FS_T<CLASS, FS_SIMD_CLASS>;
    }
    return nullptr; 
}

#define FASTSIMD_BUILD_CLASS( CLASS ) \
template CLASS* FastSIMD::ClassFactory<CLASS, FS_SIMD_CLASS::SIMD_Level>( FastSIMD::MemoryResource );

#include "../FastSIMD_BuildList.inl"
