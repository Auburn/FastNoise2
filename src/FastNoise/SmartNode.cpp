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
    class SmartNodeManagerPool
    {
    public:
        SmartNodeManagerPool( uint32_t size ) :
            mAllocState( 0 ), mNextPool( nullptr ), mPoolSize( std::min<uint32_t>( size, INT32_MAX ) )
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
                                
            } while( !mAllocState.compare_exchange_weak( allocState, newAllocState, std::memory_order_relaxed ) );

            return startSlot;
        }

        int32_t Free( const void* ptr )
        {
            if( Contains( ptr ) )
            {
                uint64_t allocState = mAllocState.fetch_sub( 1, std::memory_order_relaxed );

                assert( (uint32_t)allocState != 0 );
                return (int32_t)allocState - 1;
            }

            return -1;
        }

        int32_t AllocCount() const
        {
            return (int32_t)mAllocState.load( std::memory_order_relaxed );
        }

        bool MarkForRemoval()
        {
            uint64_t allocState = mAllocState.load( std::memory_order_relaxed );

            if( (uint32_t)allocState != 0 )
            {
                return false;
            }

            uint64_t newAllocState = ( (uint64_t)mPoolSize << 32 ) + 1; // Set as full

            return mAllocState.compare_exchange_strong( allocState, newAllocState, std::memory_order_relaxed );
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
        static inline uint32_t sNewPoolSize = 64 * 1024;

        void* Alloc( size_t size, size_t align ) 
        {
            if( void* ptr = AllocFromPools( size, align ) )
            {
                return ptr;
            }

            std::lock_guard lock( mMutex );

            if( void* ptr = AllocFromPools( size, align ) )
            {
                return ptr;
            }
      
            if( void* poolAlloc = std::malloc( std::max( (uint32_t)sizeof( SmartNodeManagerPool ), sNewPoolSize ) ) )
            {        
                SmartNodeManagerPool* newPool = new( poolAlloc ) SmartNodeManagerPool( sNewPoolSize - (uint32_t)sizeof( SmartNodeManagerPool ) );

                void* alloc = newPool->TryAlloc( size, align );
                assert( alloc ); // Alloc too large to fit in empty pool, increase pool size

                if( mPools )
                {
                    SmartNodeManagerPool* pool = mPools;

                    while( SmartNodeManagerPool* nextPool = pool->mNextPool.load( std::memory_order_relaxed ) )
                    {
                        pool = nextPool;
                    }  

                    pool->mNextPool.store( newPool, std::memory_order_release );
                }
                else
                {
                    mPools = newPool;
                }

                return alloc;
            } 

            return nullptr;
        }

        void Free( const void* ptr )
        {
            SmartNodeManagerPool* pool = mPools;

            while( pool )
            {
                int32_t allocCount = pool->Free( ptr );

                if( allocCount >= 0 )
                {
                    if( allocCount == 0 )
                    {
                        RemoveEmptyPool();
                    }
                    return;
                }

                pool = pool->mNextPool;
            }

            assert( 0 ); // Pointer not in any of the pools
        }
        
    private:
        void* AllocFromPools( size_t size, size_t align )
        {
            SmartNodeManagerPool* pool = mPools;

            while( pool )
            {
                if( void* ptr = pool->TryAlloc( size, align ) )
                {
                    return ptr;
                }

                pool = pool->mNextPool;
            }
            return nullptr;
        }

        void RemoveEmptyPool()
        {
            SmartNodeManagerPool* pool = mPools;
            SmartNodeManagerPool* emptyPool = mPools->AllocCount() > 0 ? nullptr : mPools;

            while( SmartNodeManagerPool* nextPool = pool->mNextPool.load( std::memory_order_relaxed ) )
            {
                int32_t allocCount = nextPool->AllocCount();

                if( allocCount == 0 )
                {
                    if( emptyPool ) // Only remove a pool if we have 2 empty pools
                    {
                        std::lock_guard lock( mMutex );

                        SmartNodeManagerPool* toRemove = nextPool;

                        if( toRemove->MarkForRemoval() )
                        {
                            pool->mNextPool.store( toRemove->mNextPool.load( std::memory_order_relaxed ) );

                            toRemove->~SmartNodeManagerPool();

                            std::free( toRemove );
                        }

                        return;
                    }

                    emptyPool = nextPool;                    
                }

                pool = nextPool;
            }
        }
        
        SmartNodeManagerPool* mPools = nullptr;
        mutable std::mutex mMutex;
    };

    static SmartNodeMemoryAllocator gMemoryAllocator;

    void SmartNodeManager::SetMemoryPoolSize( uint32_t size )
    {
        SmartNodeMemoryAllocator::sNewPoolSize = size;
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

#endif