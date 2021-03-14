#pragma once
#include <cstddef>
#include <utility>

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
        constexpr SmartNode( nullptr_t = nullptr )
        {
            mPtr = nullptr;
            mReferenceId = SmartNodeManager::InvalidReferenceId;
        }

        template<typename U>
        explicit SmartNode( U* ptr )
        {
            mReferenceId = SmartNodeManager::NewReference();
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
            mPtr = static_cast<T*>( node.mPtr );
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node, T* ptr )
        {
            SmartNodeManager::IncReference( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = ptr;
        }

        SmartNode( SmartNode&& node )
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


        SmartNode& operator=( SmartNode&& node )
        {
            std::swap( mPtr, node.mPtr );
            std::swap( mReferenceId, node.mReferenceId );
            return *this;
        }

        SmartNode& operator=( const SmartNode& node )
        {
            if( mReferenceId != node.mReferenceId )
            {
                SmartNodeManager::IncReference( node.mReferenceId );
                free();
            }
            mPtr = node.mPtr;
            mReferenceId = node.mReferenceId;

            return *this;
        }

        template<typename U>
        SmartNode& operator=( const SmartNode<U>& node )
        {
            if( mReferenceId != node.mReferenceId )
            {
                SmartNodeManager::IncReference( node.mReferenceId );
                free();
            }
            mPtr = static_cast<T*>( node.mPtr );
            mReferenceId = node.mReferenceId;

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
