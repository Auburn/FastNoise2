#pragma once
#include <cstddef>
#include <utility>
#include <cassert>

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
    };

    class Generator;

    template<typename T = Generator>
    class SmartNode
    {
    public:
        constexpr SmartNode( std::nullptr_t = nullptr )
        {
            mReferenceId = SmartNodeManager::InvalidReferenceId;
            mPtr = nullptr;
        }

        explicit SmartNode( T* ptr )
        {
            mReferenceId = ptr ? SmartNodeManager::NewReference() : SmartNodeManager::InvalidReferenceId;
            mPtr = ptr;
        }
        
        SmartNode( const SmartNode& node )
        {
            SmartNodeManager::IncReference( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node )
        {
            SmartNodeManager::IncReference( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node, T* ptr )
        {
            assert( ptr );

            SmartNodeManager::IncReference( node.mReferenceId );
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
            free();
        }

        SmartNode& operator=( SmartNode&& node ) noexcept
        {
            std::swap( mPtr, node.mPtr );
            std::swap( mReferenceId, node.mReferenceId );
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
                free();
                mReferenceId = node.mReferenceId;
                mPtr = node.mPtr;

                node.mReferenceId = SmartNodeManager::InvalidReferenceId;
                node.mPtr = nullptr;
            }

            return *this;
        }

        SmartNode& operator=( const SmartNode& node )
        {
            if( mReferenceId != node.mReferenceId )
            {
                SmartNodeManager::IncReference( node.mReferenceId );
                free();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

            return *this;
        }

        template<typename U>
        SmartNode& operator=( const SmartNode<U>& node )
        {
            if( mReferenceId != node.mReferenceId )
            {
                SmartNodeManager::IncReference( node.mReferenceId );
                free();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

            return *this;
        }

        T& operator*() const
        {
            return *mPtr;
        }

        T* operator->() const
        {
            return mPtr;
        }

        operator bool() const
        {
            return mPtr;
        }

        T* get() const
        {
            return mPtr;
        }

        void reset( T* ptr = nullptr )
        {
            *this = SmartNode( ptr );
        }

    private:
        void free()
        {
            if( SmartNodeManager::DecReference( mReferenceId ) )
            {
                delete mPtr;
            }
            mReferenceId = SmartNodeManager::InvalidReferenceId;
            mPtr = nullptr;
        }

        template<typename U>
        friend class SmartNode;

        T* mPtr;
        size_t mReferenceId;
    };
} // namespace FastNoise
