#pragma once
#include <cstddef>
#include <utility>
#include <cassert>
#include <type_traits>
#include <functional>

namespace FastNoise
{
    class SmartNodeManager
    {
    public:
        SmartNodeManager() = delete;

    private:
        template<typename T>
        friend class SmartNode;

        static constexpr size_t InvalidReferenceId = (size_t)-1;

        static size_t NewReference();

        static void IncReference( size_t id );

        static bool DecReference( size_t id );

        static size_t ReferenceCount( size_t id );
    };

    class Generator;

    template<typename T = Generator>
    class SmartNode
    {
        static_assert( std::is_base_of<Generator, T>::value, "SmartNode should only be used for FastNoise node classes" );

    public:
        constexpr SmartNode( std::nullptr_t = nullptr ) noexcept :
            mReferenceId( SmartNodeManager::InvalidReferenceId ),
            mPtr( nullptr )
        {}

        explicit SmartNode( T* ptr ) :
            mReferenceId( ptr ? SmartNodeManager::NewReference() : SmartNodeManager::InvalidReferenceId ),
            mPtr( ptr )
        {}
        
        SmartNode( const SmartNode& node )
        {
            try_inc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node )
        {
            try_inc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node, T* ptr )
        {
            assert( ptr );

            try_inc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = ptr;
        }

        SmartNode( SmartNode&& node ) noexcept
        {
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;

            node.mReferenceId = SmartNodeManager::InvalidReferenceId;
            node.mPtr = nullptr;
        }

        template<typename U>
        SmartNode( SmartNode<U>&& node ) noexcept
        {
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;

            node.mReferenceId = SmartNodeManager::InvalidReferenceId;
            node.mPtr = nullptr;
        }

        ~SmartNode()
        {
            release();
        }

        SmartNode& operator=( SmartNode&& node ) noexcept
        {
            swap( node );
            return *this;
        }

        template<typename U>
        SmartNode& operator=( SmartNode<U>&& node ) noexcept
        {
            if( mReferenceId == node.mReferenceId )
            {
                mPtr = node.mPtr;                
            }
            else
            {
                release();
                mReferenceId = node.mReferenceId;
                mPtr = node.mPtr;

                node.mReferenceId = SmartNodeManager::InvalidReferenceId;
                node.mPtr = nullptr;
            }

            return *this;
        }

        SmartNode& operator=( const SmartNode& node ) noexcept
        {
            if( mReferenceId != node.mReferenceId )
            {
                try_inc( node.mReferenceId );
                release();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

            return *this;
        }

        template<typename U>
        SmartNode& operator=( const SmartNode<U>& node ) noexcept
        {
            if( mReferenceId != node.mReferenceId )
            {
                try_inc( node.mReferenceId );
                release();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

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

        T& operator*() const noexcept
        {
            return *mPtr;
        }

        T* operator->() const noexcept
        {
            return mPtr;
        }

        operator bool() const noexcept
        {
            return mPtr;
        }

        T* get() const noexcept
        {
            return mPtr;
        }

        void reset( T* ptr = nullptr )
        {
            *this = SmartNode( ptr );
        }

        void swap( SmartNode& node ) noexcept
        {
            std::swap( mReferenceId, node.mReferenceId );
            std::swap( mPtr, node.mPtr );
        }

        long use_count() const noexcept
        {
            if( mReferenceId == SmartNodeManager::InvalidReferenceId )
            {
                return 0;
            }

            return (long)SmartNodeManager::ReferenceCount( mReferenceId );
        }

        bool unique() const noexcept
        {
            return use_count() == 1;
        }

    private:
        static void try_inc( size_t id )
        {
            if( id != SmartNodeManager::InvalidReferenceId )
            {
                SmartNodeManager::IncReference( id );
            }
        }

        void release()
        {
            if( mReferenceId != SmartNodeManager::InvalidReferenceId && SmartNodeManager::DecReference( mReferenceId ) )
            {
                delete mPtr;

                mReferenceId = SmartNodeManager::InvalidReferenceId;
                mPtr = nullptr;
            }
        }

        template<typename U>
        friend class SmartNode;

        size_t mReferenceId;
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
