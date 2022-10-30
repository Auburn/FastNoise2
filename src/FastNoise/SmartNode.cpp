#include <FastNoise/FastNoise_Config.h>

#if !FASTNOISE_USE_SHARED_PTR

#include <FastNoise/SmartNode.h>

#include <mutex>
#include <atomic>
#include <vector>
#include <list>
#include <cstring>
#include <memory>
#include <algorithm>

namespace FastNoise
{
    using SmartNodeReference = std::atomic<uint32_t>*;
    
    struct SmartNodeManagerPool
    {
        static constexpr uint32_t kInvalidSlot = (uint32_t)-1;

        struct SlotHeader
        {
            std::atomic<uint32_t> references;
            uint32_t size;
        };

        struct Slot
        {
            uint32_t pos;
            uint32_t size;            
        };

        SmartNodeManagerPool( uint32_t size )
        {
            size = std::min<uint32_t>( size, INT32_MAX );

            uint32_t alignOffset = size % alignof( SlotHeader );
            if( alignOffset )
            {
                // pool size needs to be multiple of `alignof( SlotHeader )` (likely 4)
                size += alignof( SlotHeader ) - alignOffset;
            }

            poolSize = size;
            pool = (uint8_t*)new SlotHeader[size / sizeof( SlotHeader )];

            freeSlots = { { 0, poolSize } };
        }

        SmartNodeManagerPool( const SmartNodeManagerPool& ) = delete;
        SmartNodeManagerPool( SmartNodeManagerPool&& ) = delete;

        ~SmartNodeManagerPool()
        {
            assert( usedSlots.empty() );

            delete[] pool;
        }

        bool Contains( const void* ptr ) const
        {
            return ptr >= pool && ptr < pool + poolSize;
        }

        auto GetUsedSlotItr( const void* ref ) const
        {
            if( ref >= pool && ref < pool + poolSize )
            {
                for( auto itr = usedSlots.begin(); itr != usedSlots.end(); ++itr )
                {
                    const uint8_t* start = pool + itr->pos;

                    if( start <= ref && start + itr->size > ref )
                    {
                        return itr;
                    }
                }
            }

            return usedSlots.end();
        }

        auto GetUsedSlotItr( uint32_t pos ) const
        {
            return std::find_if( usedSlots.begin(), usedSlots.end(), [pos]( const Slot& slot ) 
            {
                return slot.pos == pos;    
            } );
        }
        
        bool ValidatePtr( SmartNodeReference pos, const void* ptr ) const
        {            
            if( *pos == 0 )
            {
                assert( 0 );
                return false;
            }

            auto slot = GetUsedSlotItr( ptr );

            // Check pos pointing at garbage data
            if( slot == usedSlots.end() )
            {
                assert( 0 );
                return false;
            }

            // Check pos is correct
            if( pool + slot->pos != (uint8_t*)pos )
            {
                assert( 0 );
                return false;
            }
            return true;
        }

        std::atomic<uint32_t>& GetReferenceCount( uint32_t pos ) const
        {
            SlotHeader* slot = (SlotHeader*)( pool + pos );

            assert( pos < poolSize );

            return slot->references;
        }

        SmartNodeReference GetReferenceId( const void* ptr ) const
        {
            auto slot = GetUsedSlotItr( ptr );

            if( slot == usedSlots.end() )
            {
                return nullptr;
            }

            return &reinterpret_cast<SlotHeader*>( pool + slot->pos )->references ;
        }

        void* TryAlloc( size_t size, size_t align )
        {
            align = std::max( align, alignof( SlotHeader ) );

            for( uint32_t idx = 0; idx < freeSlots.size(); idx++ )
            {
                if( freeSlots[idx].size < size + sizeof( SlotHeader ) )
                {
                    continue;
                }

                uint8_t* startSlot = pool + freeSlots[idx].pos;
                void* ptr = startSlot + sizeof( SlotHeader );
                size_t space = freeSlots[idx].size - sizeof( SlotHeader );

                if( std::align( align, size, ptr, space ) )
                {                   
                    uint8_t* endSlot = (uint8_t*)ptr + size;

                    // Align next slot correctly for SlotHeader
                    size_t alignmentOffset = (size_t)endSlot % alignof( SlotHeader );

                    if( alignmentOffset )
                    {
                        endSlot += alignof( SlotHeader ) - alignmentOffset;
                    }

                    uint32_t slotSize = (uint32_t)( endSlot - startSlot );

                    assert( freeSlots[idx].size >= slotSize );
                    
                    new( startSlot ) SlotHeader { { 0u }, slotSize };
                    usedSlots.emplace_back( Slot{ freeSlots[idx].pos, slotSize } );

                    // Check if remaining free slot is empty
                    if( freeSlots[idx].size <= slotSize )
                    {
                        assert( freeSlots[idx].size == slotSize );
                        freeSlots.erase( freeSlots.cbegin() + idx );
                        return ptr;
                    }

                    freeSlots[idx].pos += slotSize;
                    freeSlots[idx].size -= slotSize;

                    return ptr;
                }
            }

            assert( freeSlots.empty() || freeSlots[0].size != poolSize ); // Empty pool not large enough to fit alloc, increase the pool size
            return nullptr;
        }

        void DeAlloc( SmartNodeReference ref )
        {
            SlotHeader* slotHeader = (SlotHeader*)( ref + offsetof( SlotHeader, references ) );
            auto slot = GetUsedSlotItr( ref );

            assert( slot != usedSlots.end() );            
            assert( slotHeader->references == 0 );
            assert( slot->size < poolSize );

            // Merge free slots as necessary
            Slot* expandedBefore = nullptr;
            uint32_t idx = 0;
            uint32_t pos = (uint32_t)((uint8_t*)ref - pool);

            for( ; idx < freeSlots.size(); idx++ )
            {
                if( freeSlots[idx].pos > pos )
                {
                    break;
                }

                // Found slot before, expand
                if( freeSlots[idx].pos + freeSlots[idx].size == pos )
                {
                    freeSlots[idx].size += slot->size;
                    expandedBefore = &freeSlots[idx];
                    idx++;
                    break;
                }
            }

            if( idx < freeSlots.size() && freeSlots[idx].pos == pos + slot->size )
            {
                // Found slot before and after, expand before again, delete after
                if( expandedBefore )
                {
                    expandedBefore->size += freeSlots[idx].size;
                    freeSlots.erase( freeSlots.begin() + idx );
                }
                else // Found slot after, expand
                {
                    freeSlots[idx].pos = pos;
                    freeSlots[idx].size += slot->size;
                }
            }
            else if( !expandedBefore ) // No slots before or after, create new
            {
                freeSlots.emplace( freeSlots.begin() + idx, Slot { pos, slot->size } );
            }
            
            slotHeader->~SlotHeader();
            assert( memset( slotHeader, 255, slot->size ) );

            usedSlots.erase( slot );
        }

        uint32_t poolSize;
        uint8_t* pool;
        std::vector<Slot> freeSlots;
        std::vector<Slot> usedSlots;
    };
    
    class SmartNodeMemoryAllocator
    {
    public:
        static inline uint32_t sNewPoolSize = 256 * 1024;

        bool ValidatePtr( SmartNodeReference ref, const void* ptr )
        {
            std::lock_guard lock( mMutex );

            for(const auto & pool : mPools)
            {
                if( pool.Contains( ptr ) )
                {
                    return pool.ValidatePtr( ref, ptr );
                }
            }
            
            return false;
        }

        SmartNodeReference GetReference( const void* ptr )
        {
            std::lock_guard lock( mMutex );

            SmartNodeReference ref = nullptr;

            for( auto& poolItr : mPools )
            {
                ref = poolItr.GetReferenceId( ptr );
                if( ref )
                {
                    return ref;
                }
            }

            // Could not find ptr in pools, probably not allocated using this class
            assert( 0 );
            return nullptr;
        }

        void* Alloc( size_t size, size_t align ) 
        {
            std::lock_guard lock( mMutex );

            if( void* ptr = AllocFromPools( size, align ) )
            {
                return ptr;
            }

            mPools.emplace_back( sNewPoolSize );

            return AllocFromPools( size, align );
        }

        void Dealloc( SmartNodeReference ref )
        {
            std::lock_guard lock( mMutex );

            
            for( auto& poolItr : mPools )
            {
                if( poolItr.Contains( ref ) )
                {
                    poolItr.DeAlloc( ref );
                    return;
                }
            }

            assert( 0 );
        }
        
    private:
        void* AllocFromPools( size_t size, size_t align )
        {
            uint32_t idx = 0;            

            for( auto& poolItr : mPools )
            {
                if( void* ptr = poolItr.TryAlloc( size, align ) )
                {
                    return ptr;
                }

                idx++;
            }
            return nullptr;
        }

        // std::list is used to allow lock free reads to pools
        // In most use cases there should only be 1 pool so performance is not a concern
        std::list<SmartNodeManagerPool> mPools;
        mutable std::mutex mMutex;
    };

    static SmartNodeMemoryAllocator gMemoryAllocator;

    void SmartNodeManager::SetMemoryPoolSize( uint32_t size )
    {
        SmartNodeMemoryAllocator::sNewPoolSize = size;
    }

    uint64_t SmartNodeManager::GetReference( const void* ptr )
    {
        assert( ptr );

        return (uint64_t)gMemoryAllocator.GetReference( ptr );
    }

    void SmartNodeManager::IncReference( uint64_t id )
    {
        assert( id != kInvalidReferenceId );

        std::atomic<uint32_t>& refCount = *(SmartNodeReference)id;

        ++refCount;
    }

    void SmartNodeManager::DecReference( uint64_t id, void* ptr, void ( *destructorFunc )( void* ) )
    {
        SmartNodeReference refCount = (SmartNodeReference)id;

        assert( gMemoryAllocator.ValidatePtr( refCount, ptr ) );   

        uint32_t previousRefCount = refCount->fetch_sub( 1 );

        assert( previousRefCount );

        if( previousRefCount == 1 )
        {
            destructorFunc( ptr );

            gMemoryAllocator.Dealloc( refCount );
        }
    }

    uint32_t SmartNodeManager::ReferenceCount( uint64_t id )
    {
        assert( id != kInvalidReferenceId );

        SmartNodeReference refCount = (SmartNodeReference)id;
        
        return *refCount;
    }

    void* SmartNodeManager::Allocate( size_t size, size_t align )
    {
        return gMemoryAllocator.Alloc( size, align );
    }
} // namespace FastNoise

#endif