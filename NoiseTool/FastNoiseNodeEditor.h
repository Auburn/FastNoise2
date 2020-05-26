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
        FastNoiseNodeEditor();
        void DoContextMenu();
        void DoNodes();
        void UpdateSelected();
        void Update();

    private:
        struct Node
        {
            using Ptr = std::unique_ptr<Node>;

            Node( const FastNoise::Metadata* );
            void GeneratePreview( FastNoiseNodeEditor* );
            std::shared_ptr<FastNoise::Generator> GetGenerator( std::vector<Ptr>&, std::vector<int>&, bool& );

            int id;
            const FastNoise::Metadata* metadata;

            std::vector<int*> memberLinks;
            std::vector<int> memberNodes;
            std::vector<std::pair<int, float>> memberHybrids;
            std::vector<FastNoise::Metadata::MemberVariable::ValueUnion> memberValues;

            static const int NoiseSize = 224;
            GL::Texture2D noiseTexture;
            ImageView2D noiseImage;
            float noiseData[NoiseSize * NoiseSize];
        };

        std::vector<Node::Ptr> mNodes;
        int mCurrentNodeId = 1;

        void GenerateSelectedPreview();

        int mSelectedNode = -1;
        GL::Texture2D mNoiseTexture;
        ImageView2D mNoiseImage;
        float* mNoiseData = nullptr;
        VectorTypeFor<2, Int> mPreviewWindowsSize = { 0,0 };

        float mNodeFrequency = 0.05f;
        int mNodeSeed = 1337;
    };
}