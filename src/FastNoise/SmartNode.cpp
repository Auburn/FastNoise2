#include <FastNoise/FastNoise_Config.h>

#if !FASTNOISE_USE_SHARED_PTR

#include <FastNoise/SmartNode.h>

#include <cstdlib>
#include <mutex>
#include <atomic>
#include <vector>
#include <list>
#include <cstring>
#include <memory>

namespace FastNoise
{
    union SmartNodeReference
    {
        uint64_t u64;
        struct
        {
            uint32_t pool;
            uint32_t id;
        } u32;
    };
    
    struct SmartNodeManagerPool
    {
        static constexpr uint32_t kInvalidSlot = (uint32_t)-1;

        struct SlotInfo
        {
            uint32_t size;
            std::atomic<uint32_t> references;
        };

        struct FreeSlot
        {
            uint32_t pos;
            uint32_t size;            
        };

        SmartNodeManagerPool( uint32_t size )
        {
            uint32_t alignOffset = size % alignof( SlotInfo );
            if( alignOffset )
            {
                // pool size needs to be multiple of `alignof( SlotInfo )` (likely 4)
                size += alignof( SlotInfo ) - alignOffset;
            }

            poolSize = size;
            pool = (uint8_t*)new SlotInfo[size / sizeof( SlotInfo )];

            freeSlots = { { 0, poolSize } };
        }

        SmartNodeManagerPool( const SmartNodeManagerPool& ) = delete;
        SmartNodeManagerPool( SmartNodeManagerPool&& ) = delete;

        ~SmartNodeManagerPool()
        {
            delete[] pool;
        }
        
        bool ValidatePtr( uint32_t pos, void* ptr ) const
        {            
            if( pos >= poolSize )
            {
                assert( 0 );
                return false;
            }

            SlotInfo* slot = (SlotInfo*)( pool + pos );

            // Check pos pointing at garbage data
            if( slot->size <= sizeof( SlotInfo ) || slot->size > poolSize )
            {
                assert( 0 );
                return false;
            }

            // ptr not contained in slot
            if( ptr < ( pool + pos + sizeof( SlotInfo ) ) || ptr >= ( pool + pos + slot->size ))
            {
                assert( 0 );
                return false;
            }
            return true;
        }

        std::atomic<uint32_t>& GetReferenceCount( uint32_t pos ) const
        {
            SlotInfo* slot = (SlotInfo*)( pool + pos );

            assert( pos < poolSize && slot->size < poolSize );

            return slot->references;
        }

        void* TryAlloc( uint32_t& pos, size_t size, size_t align )
        {
            for( uint32_t idx = 0; idx < freeSlots.size(); idx++ )
            {
                if( freeSlots[idx].size < size + sizeof( SlotInfo ) )
                {
                    continue;
                }

                void* ptr = pool + freeSlots[idx].pos + sizeof( SlotInfo );
                size_t space = freeSlots[idx].size - sizeof( SlotInfo );

                if( std::align( align, size, ptr, space ) )
                {                   
                    uint8_t* startSlot = pool + freeSlots[idx].pos;
                    uint8_t* endSlot = (uint8_t*)ptr + size;

                    // Align next slot correctly for SlotInfo
                    size_t alignmentOffset = (size_t)endSlot % alignof( SlotInfo );

                    if( alignmentOffset )
                    {
                        endSlot += alignof( SlotInfo ) - alignmentOffset;
                    }

                    uint32_t slotSize = (uint32_t)( endSlot - startSlot );

                    assert( freeSlots[idx].size >= slotSize );

                    pos = freeSlots[idx].pos;
                    freeSlots[idx].pos += slotSize;
                    freeSlots[idx].size -= slotSize;

                    // Check if remaining free slot is empty
                    if( freeSlots[idx].size == 0 )
                    {
                        freeSlots.erase( freeSlots.cbegin() + idx );
                    }

                    SlotInfo* newSlot = new( startSlot ) SlotInfo{ slotSize, (uint32_t)0 };

                    return newSlot + 1;
                }
            }

            assert( freeSlots.empty() || freeSlots[0].size != poolSize ); // Empty pool not large enough to fit alloc, increase the pool size
            return nullptr;
        }

        void DeAlloc( uint32_t pos )
        {
            SlotInfo* slot = (SlotInfo*)( pool + pos );

            assert( slot->references == 0 );
            assert( slot->size < poolSize );

            // Merge free slots as necessary
            FreeSlot* expandedBefore = nullptr;
            uint32_t idx = 0;

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
                freeSlots.emplace( freeSlots.begin() + idx, FreeSlot { pos, slot->size } );
            }
            
            assert( memset( slot, 255, slot->size ) );
            slot->~SlotInfo();
        }

        uint32_t poolSize;
        uint8_t* pool;
        std::vector<FreeSlot> freeSlots;
    };
    
    class SmartNodeMemoryAllocator
    {
    public:
        static inline uint32_t sNewPoolSize = 256 * 1024;

        static thread_local inline SmartNodeReference sLastAlloc = { SmartNodeManager::kInvalidReferenceId };

        bool ValidatePtr( SmartNodeReference ref, void* ptr )
        {
            std::lock_guard lock( mMutex );

            if( ref.u32.pool >= mPools.size() )
            {
                assert( 0 );
                return false;
            }

            return std::next( mPools.begin(), ref.u32.pool )->ValidatePtr( ref.u32.id, ptr );
        }

        std::atomic<uint32_t>& GetReferenceCount( SmartNodeReference ref ) const
        {
            return std::next( mPools.begin(), ref.u32.pool )->GetReferenceCount( ref.u32.id );
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

            std::next( mPools.begin(), ref.u32.pool )->DeAlloc( ref.u32.id );
        }
        
    private:
        void* AllocFromPools( size_t size, size_t align )
        {
            uint32_t idx = 0;            

            for( auto& poolItr : mPools )
            {
                uint32_t newId;

                if( void* ptr = poolItr.TryAlloc( newId, size, align ) )
                {
                    assert( sLastAlloc.u64 == SmartNodeManager::kInvalidReferenceId );

                    sLastAlloc.u32.pool = idx;
                    sLastAlloc.u32.id = newId;

                    return ptr;
                }

                idx++;
            }
            return nullptr;
        }

        // std::list is used to allow lock free access to pools
        // In most use cases there should only be 1 pool so performance is not a concern
        std::list<SmartNodeManagerPool> mPools;
        std::mutex mMutex;
    };

    static SmartNodeMemoryAllocator gMemoryAllocator;

    void SmartNodeManager::SetMemoryPoolSize( uint32_t size )
    {
        SmartNodeMemoryAllocator::sNewPoolSize = size;
    }

    uint64_t SmartNodeManager::GetLastAllocID( void* ptr )
    {
        uint64_t id = SmartNodeMemoryAllocator::sLastAlloc.u64;

        assert( gMemoryAllocator.ValidatePtr( { id }, ptr ) );

        SmartNodeMemoryAllocator::sLastAlloc.u64 = kInvalidReferenceId;
        return id;
    }

    void SmartNodeManager::IncReference( uint64_t id )
    {
        assert( id != kInvalidReferenceId );

        std::atomic<uint32_t>& refCount = gMemoryAllocator.GetReferenceCount( { id } );

        ++refCount;
    }

    void SmartNodeManager::DecReference( uint64_t id, void* ptr, void ( *destructorFunc )( void* ) )
    {
        assert( gMemoryAllocator.ValidatePtr( { id }, ptr ) );

        std::atomic<uint32_t>& refCount = gMemoryAllocator.GetReferenceCount( { id } );    

        uint32_t previousRefCount = refCount.fetch_sub( 1 );

        assert( previousRefCount );

        if( previousRefCount == 1 )
        {
            destructorFunc( ptr );

            gMemoryAllocator.Dealloc( { id } );
        }
    }

    uint32_t SmartNodeManager::ReferenceCount( uint64_t id )
    {
        assert( id != kInvalidReferenceId );
        
        return gMemoryAllocator.GetReferenceCount( { id } );
    }

    void* SmartNodeManager::Allocate( size_t size, size_t align )
    {
        return gMemoryAllocator.Alloc( size, align );
    }
} // namespace FastNoise

#endif