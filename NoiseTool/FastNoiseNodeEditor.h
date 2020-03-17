#pragma once
#include <vector>
#include <memory>

#include <Magnum/Magnum.h>
#include <Magnum\GL\GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum\ImageView.h>

#include "FastNoise/FastNoise.h"

namespace Magnum
{
    class FastNoiseNodeEditor
    {
    public:
        void Update();


    private:
        struct Node
        {
            using Ptr = std::unique_ptr<Node>;

            Node( const FastNoise::Metadata* );
            void GeneratePreview( std::vector<Ptr>& );
            std::shared_ptr<FastNoise::Generator> GetGenerator( std::vector<Ptr>&, bool& );

            int id;
            const FastNoise::Metadata* metadata;

            std::vector<int> memberNodes;
            std::vector<FastNoise::Metadata::MemberVariable::ValueUnion> memberValues;

            static const int NoiseSize = 192;
            GL::Texture2D noiseTexture;
            ImageView2D noiseImage;
            float noiseData[NoiseSize * NoiseSize];
        };

        std::vector<Node::Ptr> mNodes;
        int mCurrentNodeId = 1;
    };
}