#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#include <cassert>
#include <type_traits>
#include <functional>

#include "Config.h"

namespace FastNoise
{
    /** @brief Manages the memory pool used by SmartNode for allocating generator nodes.
     *
     *  All FastNoise generator nodes are allocated from an internal memory pool for
     *  cache-friendly layout and fast allocation. This class is not directly instantiated;
     *  use SetMemoryPoolSize() before creating nodes if you need to adjust the pool.
     */
    class FASTNOISE_API SmartNodeManager
    {
    public:
        static constexpr uint64_t kInvalidReferenceId = (uint64_t)0;

        SmartNodeManager() = delete;

        /** @brief Set the size of the node memory pool in bytes.
         *
         *  Call this before creating any nodes if the default pool size is insufficient.
         *  @param size  Pool size in bytes.
         */
        static void SetMemoryPoolSize( uint32_t size );

    private:
        template<typename>
        friend struct MetadataT;

        template<typename>
        friend class SmartNode;

        template<typename T>
        friend SmartNode<T> New( FastSIMD::FeatureSet );

        static void* Allocate( size_t size, size_t align );

        static void Free( const void* ptr );
    };

    namespace Internal
    {
        SmartNode<> ToSmartNode( const void* );
    }

    /** @brief Intrusive reference-counted smart pointer for FastNoise generator nodes.
     *
     *  SmartNode manages the lifetime of Generator-derived objects. It behaves similarly
     *  to std::shared_ptr but uses an intrusive reference count embedded in the Generator
     *  and allocates from a dedicated memory pool for better cache performance.
     *
     *  Nodes are created via FastNoise::New<T>() or FastNoise::NewFromEncodedNodeTree(),
     *  both of which return a SmartNode. SmartNode<> (defaulting to T=Generator) can hold
     *  any node type, acting as a type-erased handle.
     *
     *  @code
     *  auto simplex = FastNoise::New<FastNoise::Simplex>();  // SmartNode<Simplex>
     *  FastNoise::SmartNode<> generic = simplex;             // implicit upcast to SmartNode<Generator>
     *  @endcode
     *
     *  @tparam T  Generator-derived type. Defaults to Generator for type-erased usage.
     */
    template<typename T>
    class SmartNode
    {
    public:
        static_assert( std::is_base_of<Generator, T>::value, "SmartNode should only be used for FastNoise node classes" );

        /** @brief Attempt a dynamic cast from another SmartNode type.
         *  @tparam U  Source node type.
         *  @param  node  Source SmartNode to cast from.
         *  @return A SmartNode<T> pointing to the same object if the cast succeeds, or nullptr.
         */
        template<typename U>
        static SmartNode DynamicCast( const SmartNode<U>& node )
        {
            if( T* dynamicCast = dynamic_cast<T*>( node.mPtr ) )
            {
                return FastNoise::SmartNode<T>( dynamicCast );
            }

            return nullptr;
        }

        /** @brief Construct an empty (null) SmartNode. */
        constexpr SmartNode( std::nullptr_t = nullptr ) noexcept :
            mPtr( nullptr )
        {}
        
        SmartNode( const SmartNode& node ) noexcept :
            mPtr( node.mPtr )
        {
            TryInc( mPtr );
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node ) noexcept :
            mPtr( node.mPtr )
        {
            TryInc( mPtr );
        }

        template<typename U>
        SmartNode( const SmartNode<U>&, T* ptr ) noexcept :
            mPtr( ptr )
        {
            TryInc( mPtr );
        }

        SmartNode( SmartNode&& node ) noexcept :
            mPtr( node.mPtr )
        {
            node.mPtr = nullptr;
        }

        template<typename U>
        SmartNode( SmartNode<U>&& node ) noexcept :
            mPtr( node.mPtr )
        {
            node.mPtr = nullptr;
        }

        ~SmartNode()
        {
            Release();
        }

        SmartNode& operator=( SmartNode&& node ) noexcept
        {
            swap( node );
            return *this;
        }

        template<typename U>
        SmartNode& operator=( SmartNode<U>&& node ) noexcept
        {
            if( mPtr != node.mPtr )            
            {
                Release();
                mPtr = node.mPtr;

                node.mPtr = nullptr;
            }

            return *this;
        }

        SmartNode& operator=( const SmartNode& node ) noexcept
        {
            if( mPtr != node.mPtr )
            {
                TryInc( node.mPtr );
                Release();
                mPtr = node.mPtr;
            }

            return *this;
        }

        template<typename U>
        SmartNode& operator=( const SmartNode<U>& node ) noexcept
        {
            if( mPtr != node.mPtr )
            {
                TryInc( node.mPtr );
                Release();
                mPtr = node.mPtr;
            }

            return *this;
        }

        template<typename U>
        friend bool operator==( const SmartNode& lhs, const SmartNode<U>& rhs ) noexcept
        {
            return lhs.get() == rhs.get();
        }

        template<typename U>
        friend bool operator!=( const SmartNode& lhs, const SmartNode<U>& rhs ) noexcept
        {
            return lhs.get() != rhs.get();
        }

        const T& operator*() const noexcept
        {
            assert( mPtr->ReferencesFetchAdd() );
            return *mPtr;
        }

        T& operator*() noexcept
        {
            assert( mPtr->ReferencesFetchAdd() );
            return *mPtr;
        }

        const T* operator->() const noexcept
        {
            assert( mPtr->ReferencesFetchAdd() );
            return mPtr;
        }

        T* operator->() noexcept
        {
            assert( mPtr->ReferencesFetchAdd() );
            return mPtr;
        }

        /** @brief Check if this SmartNode holds a valid (non-null) node. */
        explicit operator bool() const noexcept
        {
            return mPtr != nullptr;
        }

        /** @brief Get a const raw pointer to the managed node, or nullptr. */
        const T* get() const noexcept
        {
            return mPtr;
        }

        /** @brief Get a raw pointer to the managed node, or nullptr. */
        T* get() noexcept
        {
            return mPtr;
        }

        /** @brief Release the current node and optionally take ownership of a new raw pointer. */
        void reset( T* ptr = nullptr )
        {
            *this = SmartNode( ptr );
        }

        /** @brief Swap the managed node with another SmartNode. */
        void swap( SmartNode& node ) noexcept
        {
            std::swap( mPtr, node.mPtr );
        }

        /** @brief Get the current reference count.
         *  @return Number of SmartNode instances sharing ownership, or 0 if null.
         */
        long use_count() const noexcept
        {
            if( mPtr )
            {
                return mPtr->ReferencesFetchAdd();
            }

            return 0;
        }

        /** @brief Check if this is the sole owner of the managed node. */
        bool unique() const noexcept
        {
            return use_count() == 1;
        }

    private:
        template<typename U>
        friend SmartNode<U> New( FastSIMD::FeatureSet );

        template<typename U>
        friend struct MetadataT;

        template<typename U>
        friend class SmartNode;

        friend SmartNode<> Internal::ToSmartNode( const void* );

        friend T;

        explicit SmartNode( T* ptr ) :
            mPtr( ptr )
        {
            TryInc( ptr );
        }

        void Release()
        {
            using U = typename std::remove_const<T>::type;

            if( mPtr )
            {
                int32_t previousRefCount = mPtr->ReferencesFetchAdd( -1 );

                assert( previousRefCount );

                if( previousRefCount == 1 )
                {
                    const_cast<U*>( mPtr )->~U();

                    SmartNodeManager::Free( mPtr );
                }
            }

            mPtr = nullptr;
        }

        template<typename U>
        static void TryInc( U* ptr ) noexcept
        {
            if( ptr )
            {
                ptr->ReferencesFetchAdd( 1 );
            }
        }
        
        T* mPtr;
    };
} // namespace FastNoise

namespace std
{
    template<typename T>
    struct hash<FastNoise::SmartNode<T>>
    {
        size_t operator()( const FastNoise::SmartNode<T>& node ) const noexcept
        {
            return std::hash<T*>( node.get() );
        }        
    };
}
