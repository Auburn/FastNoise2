#pragma once
#include <vector>
#include <memory>

#include <Magnum/Magnum.h>
#include <Magnum\GL\GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum\ImageView.h>

#include "FastNoise/FastNoise.h"
#include "MeshNoisePreview.h"

namespace Magnum
{
    class FastNoiseNodeEditor
    {
    public:
        FastNoiseNodeEditor();
        void DoContextMenu();
        void DoNodes();
        void UpdateSelected();
        void Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition );

    private:
        struct Node
        {
            using Ptr = std::unique_ptr<Node>;

            Node( FastNoiseNodeEditor&, const FastNoise::Metadata* );
            void GeneratePreview();
            std::shared_ptr<FastNoise::Generator> GetGenerator( std::unordered_set<int>& dependancies, std::vector<std::unique_ptr<FastNoise::NodeData>>& nodeDatas );

            FastNoiseNodeEditor& editor;

            int id;
            const FastNoise::Metadata* metadata;
            std::string serialised;

            std::vector<int*> memberLinks;
            std::vector<int> memberNodes;
            std::vector<std::pair<int, float>> memberHybrids;
            std::vector<FastNoise::Metadata::MemberVariable::ValueUnion> memberValues;

            static const int NoiseSize = 224;
            GL::Texture2D noiseTexture;
            ImageView2D noiseImage;
            float noiseData[NoiseSize * NoiseSize];
        };

        std::shared_ptr<FastNoise::Generator> GenerateSelectedPreview();
        void ChangeSelectedNode( int newId );

        std::unordered_map<int, Node::Ptr> mNodes;
        int mCurrentNodeId = 0;

        MeshNoisePreview mMeshNoisePreview;

        // Preview Window
        int mSelectedNode = 0;
        GL::Texture2D mNoiseTexture;
        ImageView2D mNoiseImage;
        std::vector<float> mNoiseData;
        VectorTypeFor<2, Int> mPreviewWindowsSize = { 0,0 };

        float mNodeFrequency = 0.01f;
        int mNodeSeed = 1337;

        float mPreviewFrequency = 0.02f;
    };
}