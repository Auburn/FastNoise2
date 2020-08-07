#pragma once
#include <vector>
#include <memory>

#include <Magnum/Magnum.h>
#include <Magnum\GL\GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum\ImageView.h>

#include "FastNoise/FastNoise.h"
#include "MeshNoisePreview.h"
#include "NoiseTexture.h"

namespace Magnum
{
    class FastNoiseNodeEditor
    {
    public:
        FastNoiseNodeEditor();
        void Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition );        
        void SetSIMDLevel( FastSIMD::eLevel lvl );

        static const char* GetSIMDLevelName( FastSIMD::eLevel lvl );

    private:
        struct Node
        {
            Node( FastNoiseNodeEditor&, FastNoise::NodeData* nodeData );
            Node( FastNoiseNodeEditor&, std::unique_ptr<FastNoise::NodeData>&& nodeData );
            void GeneratePreview( bool nodeTreeChanged = true );
            std::vector<int> GetNodeIDLinks();
            void SetNodeLink( int attributeId, FastNoise::NodeData* nodeData );
            void AutoPositionChildNodes( ImVec2 nodePos, float verticalSpacing = 380.0f );

            static int GetNodeID( const FastNoise::NodeData* nodeData )
            {
                return (int)((intptr_t)nodeData / sizeof( FastNoise::NodeData )) & (INT_MAX >> 3);
            }

            int GetNodeID()
            {
                return GetNodeID( data.get() );
            }

            static int GetNodeID( int attributeId )
            {
                return (int)((unsigned int)attributeId >> 4);
            }

            int GetStartingAttributeID()
            {
                return GetNodeID() << 4;
            }

            static int GetOutputAttributeId( int nodeId )
            {
                return (nodeId << 4) | 15;
            }


            FastNoiseNodeEditor& editor;
            std::unique_ptr<FastNoise::NodeData> data;
            std::string serialised;

            static const int NoiseSize = 224;
            GL::Texture2D noiseTexture;
        };

        Node& AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata );
        FastNoise::SmartNode<> GenerateSelectedPreview();
        void ChangeSelectedNode( int newId );
        void CheckLinks();
        void DoContextMenu();
        void DoNodes();
        void UpdateSelected();

        std::unordered_map<int, Node> mNodes;
        FastNoise::NodeData* mDroppedLinkNodeId = nullptr;
        bool mDroppedLink = false;

        ImVec2 mContextStartPos;
        char mImportNodeString[1024];
        bool mImportNodeModal = false;

        MeshNoisePreview mMeshNoisePreview;
        NoiseTexture mNoiseTexture;

        int mSelectedNode = 0;
        float mNodeFrequency = 0.02f;
        int mNodeSeed = 1337;

        FastSIMD::eLevel mMaxSIMDLevel = FastSIMD::Level_Null;
        FastSIMD::eLevel mActualSIMDLevel = FastSIMD::Level_Null;
    };
}