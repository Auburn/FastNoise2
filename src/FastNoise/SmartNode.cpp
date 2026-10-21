#include <FastNoise/Utility/Config.h>
#include <FastNoise/Utility/SmartNode.h>

#include <mutex>
#include <atomic>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>

namespace FastNoise
{
    class SmartNodeManagerPool
    {
    public:
        SmartNodeManagerPool( size_t size ) :
            mAllocState( 0 ), mNextPool( nullptr ), mPoolSize( (uint32_t)std::min<size_t>( size, INT32_MAX ) )
        { }

        SmartNodeManagerPool( const SmartNodeManagerPool& ) = delete;
        SmartNodeManagerPool( SmartNodeManagerPool&& ) = delete;
        
        bool Contains( const void* ptr ) const
        {
            uint8_t* pool = GetPool();
            uint32_t nextFreeIndex = (uint32_t)( mAllocState.load( std::memory_order_relaxed ) >> 32 );

            return ptr >= pool && ptr < pool + ( nextFreeIndex - 1 );
        }

        void* TryAlloc( size_t size, size_t align )
        {
            uint8_t* pool = GetPool();
            uint64_t allocState = mAllocState.load( std::memory_order_relaxed );
            uint64_t newAllocState;
            void* startSlot;

            do
            {
                uint32_t activeAllocs = (uint32_t)allocState;
                uint32_t nextFreeIndex = (uint32_t)(allocState >> 32);

                // Reset pool counter if there are no allocs
                startSlot    = activeAllocs ? pool      + nextFreeIndex : pool;
                size_t space = activeAllocs ? mPoolSize - nextFreeIndex : mPoolSize;

                if( !std::align( align, size, startSlot, space ) )
                {
                    return nullptr;
                }

                nextFreeIndex = static_cast<uint32_t>( ( (uint8_t*)startSlot + size ) - pool );
                activeAllocs++;

                newAllocState = (uint64_t)activeAllocs | ( (uint64_t)nextFreeIndex << 32 );

                // Acquire pairs with the release in Free(), so a slot reused after a reset is done with by its previous node
            } while( !mAllocState.compare_exchange_weak( allocState, newAllocState, std::memory_order_acquire, std::memory_order_relaxed ) );

            return startSlot;
        }

        int32_t Free( const void* ptr )
        {
            if( Contains( ptr ) )
            {
                uint64_t allocState = mAllocState.fetch_sub( 1, std::memory_order_release );

                assert( (uint32_t)allocState != 0 );
                return (int32_t)allocState - 1;
            }

            return -1;
        }

        int32_t AllocCount() const
        {
            // Acquire pairs with the release in Free(), so freed nodes are done with the pool before it is released
            return (int32_t)mAllocState.load( std::memory_order_acquire );
        }

        uint8_t* GetPool() const
        {
            return (uint8_t*)this + sizeof( SmartNodeManagerPool );
        }

        std::atomic<uint64_t> mAllocState;
        std::atomic<SmartNodeManagerPool*> mNextPool;
        uint32_t mPoolSize;
    };
    
    class SmartNodeMemoryAllocator
    {
    public:
        static inline std::atomic<uint32_t> sNewPoolSize = 64 * 1024;

        void* Alloc( size_t size, size_t align )
        {
            if( void* ptr = AllocFromPools( size, align ) )
            {
                return ptr;
            }

            std::lock_guard lock( mMutex );

            // Another thread may have added a pool while this one waited for the lock
            if( void* ptr = AllocFromPools( size, align ) )
            {
                return ptr;
            }

            // New pool must at least fit its header and this alloc, even if the pool size is set smaller
            size_t newPoolSize = std::max<size_t>( sNewPoolSize.load( std::memory_order_relaxed ), sizeof( SmartNodeManagerPool ) + size + align );

            void* poolAlloc = std::malloc( newPoolSize );

            if( !poolAlloc )
            {
                return nullptr;
            }

            SmartNodeManagerPool* newPool = new( poolAlloc ) SmartNodeManagerPool( newPoolSize - sizeof( SmartNodeManagerPool ) );

            void* alloc = newPool->TryAlloc( size, align );
            assert( alloc ); // New pool is sized to always fit the alloc

            // New pools go at the head, so the next allocs try them first instead of walking past full pools
            // Lock-free walks already in progress started below the new head, so they just miss this pool
            // Release so lock-free walks of the list see the pool initialised
            newPool->mNextPool.store( mPools.load( std::memory_order_relaxed ), std::memory_order_relaxed );
            mPools.store( newPool, std::memory_order_release );
            return alloc;
        }

        void Free( const void* ptr )
        {
            for( SmartNodeManagerPool* pool = mPools.load( std::memory_order_acquire ); pool; pool = pool->mNextPool.load( std::memory_order_acquire ) )
            {
                if( pool->Free( ptr ) >= 0 )
                {
                    return;
                }
            }

            assert( 0 ); // Pointer not in any of the pools
        }

        // Caller guarantees no other thread is in Alloc() or Free(), they walk the list without the lock
        void ReleaseEmptyPools()
        {
            std::lock_guard lock( mMutex );

            std::atomic<SmartNodeManagerPool*>* link = &mPools;

            while( SmartNodeManagerPool* pool = link->load( std::memory_order_acquire ) )
            {
                if( pool->AllocCount() == 0 )
                {
                    link->store( pool->mNextPool.load( std::memory_order_acquire ), std::memory_order_release );

                    pool->~SmartNodeManagerPool();
                    std::free( pool );
                }
                else
                {
                    link = &pool->mNextPool;
                }
            }
        }

    private:
        // Allocs from the first pool with room
        void* AllocFromPools( size_t size, size_t align )
        {
            for( SmartNodeManagerPool* pool = mPools.load( std::memory_order_acquire ); pool; pool = pool->mNextPool.load( std::memory_order_acquire ) )
            {
                if( void* ptr = pool->TryAlloc( size, align ) )
                {
                    return ptr;
                }
            }
            return nullptr;
        }

        // Alloc() and Free() walk the list without the lock, so pools are only freed by ReleaseEmptyPools()
        // Empty pools reset and get reused until then
        std::atomic<SmartNodeManagerPool*> mPools = nullptr;
        std::mutex mMutex; // Serialises adding and removing pools
    };

    static SmartNodeMemoryAllocator gMemoryAllocator;

    void SmartNodeManager::SetMemoryPoolSize( uint32_t size )
    {
        SmartNodeMemoryAllocator::sNewPoolSize.store( size, std::memory_order_relaxed );
    }

    void SmartNodeManager::ReleaseEmptyPools()
    {
        gMemoryAllocator.ReleaseEmptyPools();
    }

    void* SmartNodeManager::Allocate( size_t size, size_t align )
    {
        return gMemoryAllocator.Alloc( size, align );
    }

    void SmartNodeManager::Free( const void* ptr )
    {
        gMemoryAllocator.Free( ptr );        
    }
} // namespace FastNoise
