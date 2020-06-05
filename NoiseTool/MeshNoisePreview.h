#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Shaders/Phong.h>

#include "FastNoise/FastNoise.h"

namespace Magnum
{
    class MeshNoisePreview
    {
    public:
        MeshNoisePreview();

        void ReGenerate( const std::shared_ptr<FastNoise::Generator>& generator, int32_t seed );

        void Draw( const Matrix4& transformation, const Matrix4& projection );

    private:
        template<typename T>
        class GenerateQueue
        {
        public:
            void Clear()
            {
                std::unique_lock<std::mutex> lock( mMutex );
                mQueue = std::queue<T>();
            }

            T Pop()
            {
                std::unique_lock<std::mutex> lock( mMutex );
                while( mQueue.empty() )
                {
                    mCond.wait( lock );
                }
                auto item = mQueue.front();
                mQueue.pop();
                return item;
            }

            void Push( const T& item )
            {
                std::unique_lock<std::mutex> lock( mMutex );
                mQueue.push( item );
                lock.unlock();
                mCond.notify_one();
            }

        private:
            std::queue<T> mQueue;
            std::mutex mMutex;
            std::condition_variable mCond;
        };

        template<typename T>
        class CompleteQueue
        {
        public:
            uint32_t IncVersion()
            {
                std::unique_lock<std::mutex> lock( mMutex );
                return ++mVersion;
            }

            bool Pop( T& out )
            {
                std::unique_lock<std::mutex> lock( mMutex );
                if( mQueue.empty() )
                {
                    return false;
                }
                out = mQueue.front();
                mQueue.pop();
                return true;
            }

            bool Push( const T& item, uint32_t version )
            {
                std::unique_lock<std::mutex> lock( mMutex );
                if( version == mVersion )
                {
                    mQueue.push( item );
                    return true;
                }
                return false;
            }

        private:
            uint32_t mVersion = 0;
            std::queue<T> mQueue;
            std::mutex mMutex;
        };

        class Chunk
        {
        public:
            struct VertexData
            {
                VertexData() = default;
                VertexData( Vector3 p, Vector3 n, Vector3 c ) : pos(p), norm(n), col(c) {}

                Vector3 pos;
                Vector3 norm;
                Vector3 col;
            };

            struct MeshData
            {
                MeshData() = default;

                MeshData( Vector3i p, const std::vector<VertexData>& v, const std::vector<uint32_t>& i ) : pos( p )
                {
                    VertexData* vertexDataPtr = new VertexData[v.size()];
                    uint32_t* indiciesPtr = new uint32_t[i.size()];

                    std::memcpy( vertexDataPtr, v.data(), v.size() * sizeof( VertexData ) );
                    std::memcpy( indiciesPtr, i.data(), i.size() * sizeof( uint32_t ) );

                    vertexData = Containers::ArrayView<VertexData>( vertexDataPtr, v.size() );
                    indicies = Containers::ArrayView<uint32_t>( indiciesPtr, i.size() );
                }

                void Free()
                {
                    delete[] vertexData.data();
                    delete[] indicies.data();

                    vertexData = Containers::ArrayView<VertexData>();
                    indicies = Containers::ArrayView<uint32_t>();
                }

                Vector3i pos;
                Containers::ArrayView<VertexData> vertexData;
                Containers::ArrayView<uint32_t> indicies;
            };

            struct BuildData
            {
                std::shared_ptr<FastNoise::Generator> generator;
                Vector3i pos;
                float frequency, isoSurface;
                int32_t seed;
                uint32_t genVersion;
            };

            static MeshData BuildMeshData( const BuildData& buildData );

            Chunk( MeshData& meshData );

            GL::Mesh& GetMesh() { return mMesh; }

            static constexpr uint32_t SIZE = 62;
            static constexpr float AO_STRENGTH = 0.6f;

        private:
            static void AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
                int32_t facingIdx, int32_t offsetA, int32_t offsetB, Vector3 normal, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 );

            static constexpr uint32_t SIZE_GEN = SIZE + 2;

            Vector3i mPos;
            GL::Buffer mIndexBuffer, mVertexBuffer;
            GL::Mesh mMesh;
        };

        static void GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue );

        Chunk::BuildData mBuildData;
        std::vector<Chunk> mChunks;

        GenerateQueue<Chunk::BuildData> mGenerateQueue;
        CompleteQueue<Chunk::MeshData> mCompleteQueue;
        std::vector<std::thread> mThreads;

        Shaders::Phong mShader{ Shaders::Phong::Flag::VertexColor };
    };
}
