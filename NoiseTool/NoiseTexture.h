#pragma once
#include <vector>
#include <memory>
#include <thread>
#include <cstring>

#include <Magnum/Magnum.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/ImageView.h>

#include "FastNoise/FastNoise.h"
#include "MultiThreadQueues.h"

namespace Magnum
{
    class NoiseTexture
    {
    public:
        NoiseTexture();
        ~NoiseTexture();

        void Draw();
        void ReGenerate( FastNoise::SmartNodeArg<> generator );


    private:
        struct BuildData
        {
            FastNoise::SmartNode<const FastNoise::Generator> generator;
            Vector2i size;
            Vector3 offset;
            float frequency;
            int32_t seed;
            uint64_t iteration;

            enum
            {
                GenType_2D,
                GenType_2DTiled,
                GenType_3D
            } generationType;            
        };

        struct TextureData
        {
            TextureData() = default;

            TextureData( uint64_t iter, Vector2i s, FastNoise::OutputMinMax mm, const std::vector<float>& v ) : minMax( mm ), size( s ), iteration( iter )
            {
                if( v.empty() )
                {
                    return;
                }

                uint32_t* texDataPtr = new uint32_t[v.size()];

                std::memcpy( texDataPtr, v.data(), v.size() * sizeof( float ) );

                textureData = { texDataPtr, v.size() };
            }

            void Free()
            {
                delete[] textureData.data();

                textureData = nullptr;
            }

            Containers::ArrayView<uint32_t> textureData;
            FastNoise::OutputMinMax minMax;
            Vector2i size;
            uint64_t iteration;
        };

        static TextureData BuildTexture( const BuildData& buildData );
        static void GenerateLoopThread( GenerateQueue<BuildData>& generateQueue, CompleteQueue<TextureData>& completeQueue );

        void SetPreviewTexture( ImageView2D& imageView );

        GL::Texture2D mNoiseTexture;
        uint64_t mCurrentIteration = 0;

        BuildData mBuildData;
        FastNoise::OutputMinMax mMinMax;

        std::vector<std::thread> mThreads;
        GenerateQueue<BuildData> mGenerateQueue;
        CompleteQueue<TextureData> mCompleteQueue;
    };
}