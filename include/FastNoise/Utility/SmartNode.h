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
    class FASTNOISE_API SmartNodeManager
    {
    public:
        static constexpr uint64_t kInvalidReferenceId = (uint64_t)0;

        SmartNodeManager() = delete;

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

    template<typename T>
    class SmartNode
    {
    public:
        static_assert( std::is_base_of<Generator, T>::value, "SmartNode should only be used for FastNoise node classes" );

        template<typename U>
        static SmartNode DynamicCast( const SmartNode<U>& node )
        {
            if( T* dynamicCast = dynamic_cast<T*>( node.mPtr ) )
            {
                return FastNoise::SmartNode<T>( dynamicCast );
            }

            return nullptr;
        }

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

        explicit operator bool() const noexcept
        {
            return mPtr != nullptr;
        }

        const T* get() const noexcept
        {
            return mPtr;
        }

        T* get() noexcept
        {
            return mPtr;
        }

        void reset( T* ptr = nullptr )
        {
            *this = SmartNode( ptr );
        }

        void swap( SmartNode& node ) noexcept
        {
            std::swap( mPtr, node.mPtr );
        }

        long use_count() const noexcept
        {
            if( mPtr )
            {
                return mPtr->ReferencesFetchAdd();
            }

            return 0;
        }

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
