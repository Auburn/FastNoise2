#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <vector>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/VertexColor.h>

#include "FastNoise/FastNoise.h"

namespace Magnum
{
    class MeshNoisePreview
    {
    public:
        MeshNoisePreview();

        void ReGenerate( const std::shared_ptr<FastNoise::Generator>& generator, int32_t seed );

        void Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition );

    private:
        template<typename T>
        class GenerateQueue
        {
        public:
            void Clear()
            {
                std::unique_lock<std::mutex> lock( mMutex );
                mQueue = {};
            }

            size_t Count()
            {
                return mQueue.size();
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

        class VertexColorShader : public GL::AbstractShaderProgram
        {
        public:
            typedef Shaders::Generic<3>::Position Position;
            typedef Shaders::Generic<3>::Color3 Color3;
            typedef Shaders::Generic<3>::Color4 Color4;

            enum : UnsignedInt
            {
                ColorOutput = Shaders::Generic<3>::ColorOutput
            };

            explicit VertexColorShader();
            explicit VertexColorShader( NoCreateT ) noexcept : AbstractShaderProgram{ NoCreate } {}

            VertexColorShader( const VertexColorShader& ) = delete;
            VertexColorShader( VertexColorShader&& ) noexcept = default;
            VertexColorShader& operator=( const VertexColorShader& ) = delete;
            VertexColorShader& operator=( VertexColorShader&& ) noexcept = default;

            VertexColorShader& setTransformationProjectionMatrix( const Matrix4& matrix );

        private:
            Int mTransformationProjectionMatrixUniform{ 0 };
        };

        class Chunk
        {
        public:
            struct VertexData
            {
                VertexData() = default;
                VertexData( Vector3 p, Color3 c ) : pos(p), col(c) {}

                Vector3 pos;
                Color3 col;
            };

            struct MeshData
            {
                MeshData() = default;

                MeshData( Vector3i p, const std::vector<VertexData>& v, const std::vector<uint32_t>& i ) : pos( p )
                {
                    if( v.empty() )
                    {
                        return;
                    }

                    VertexData* vertexDataPtr = new VertexData[v.size()];
                    uint32_t* indiciesPtr = new uint32_t[i.size()];

                    std::memcpy( vertexDataPtr, v.data(), v.size() * sizeof( VertexData ) );
                    std::memcpy( indiciesPtr, i.data(), i.size() * sizeof( uint32_t ) );

                    vertexData = { vertexDataPtr, v.size() };
                    indicies = { indiciesPtr, i.size() };
                }

                void Free()
                {
                    delete[] vertexData.data();
                    delete[] indicies.data();

                    vertexData = nullptr;
                    indicies = nullptr;
                }

                Vector3i pos;
                Containers::ArrayView<VertexData> vertexData;
                Containers::ArrayView<uint32_t> indicies;
            };

            struct BuildData
            {
                std::shared_ptr<FastNoise::Generator> generator;
                Vector3i pos;
                Color3 color;
                float frequency, isoSurface;
                int32_t seed;
                uint32_t genVersion;
            };

            static MeshData BuildMeshData( const BuildData& buildData );

            Chunk( MeshData& meshData );

            GL::Mesh& GetMesh() { return mMesh; }
            Vector3i GetPos() const { return mPos; }

            static constexpr uint32_t SIZE = 94;
            static constexpr Vector3  LIGHT_DIR     = { 3, 4, 2 };
            static constexpr float    AMBIENT_LIGHT = 0.3f;
            static constexpr float    AO_STRENGTH   = 0.6f;

        private:
            static void AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
                int32_t facingIdx, int32_t offsetA, int32_t offsetB, Color3 color, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 );

            static constexpr uint32_t SIZE_GEN = SIZE + 2;

            Vector3i mPos;
            GL::Buffer mVertexBuffer{ NoCreate };
            GL::Buffer mIndexBuffer{ NoCreate };
            GL::Mesh mMesh{ NoCreate };
        };

        struct Vector3iHash
        {
            size_t operator()( const Vector3i& v ) const
            {
                return (size_t)v.x() ^ ((size_t)v.y() << 16) ^ ((size_t)v.z() << 32);
            }
        };

        static void GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue );

        void UpdateChunksForPosition( Vector3 position );

        std::unordered_map<Vector3i, Chunk, Vector3iHash> mChunks;
        std::unordered_set<Vector3i, Vector3iHash> mInProgressChunks;
        std::vector<Vector3i> mDistanceOrderedChunks;

        Chunk::BuildData mBuildData;
        float mLoadRange = 300.0f;
        uint32_t mTriLimit = 80000000; // 80 mil
        uint32_t mTriCount = 0;

        GenerateQueue<Chunk::BuildData> mGenerateQueue;
        CompleteQueue<Chunk::MeshData> mCompleteQueue;
        std::vector<std::thread> mThreads;

        VertexColorShader mShader;
    };
}
