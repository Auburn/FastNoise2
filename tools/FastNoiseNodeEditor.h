#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <climits>

#include <Magnum/Magnum.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/ImageView.h>

#include "FastNoise/FastNoise.h"
#include "FastNoise/Metadata.h"

#include "MeshNoisePreview.h"
#include "NoiseTexture.h"

namespace Magnum
{
    class NodeEditorApp;

    class FastNoiseNodeEditor
    {
    public:
        FastNoiseNodeEditor( NodeEditorApp* nodeEditorApp );
        ~FastNoiseNodeEditor();

        void Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition );
        void SetSIMDLevel( FastSIMD::FeatureSet lvl );
        void DoIpcPolling();

        static uintptr_t SetupIpcSocket();

    private:
        struct Node
        {
            Node( FastNoiseNodeEditor& editor, FastNoise::NodeData* nodeData, bool generatePreview = true, int id = 0 );
            Node( FastNoiseNodeEditor& editor, std::unique_ptr<FastNoise::NodeData>&& nodeData, bool generatePreview = true, int id = 0 );
            void GeneratePreview( bool nodeTreeChanged = true, bool benchmark = false );
            std::vector<FastNoise::NodeData*> GetNodeIDLinks();
            int64_t GetLocalGenerateNs();
            FastNoise::NodeData*& GetNodeLink( int attributeId );
            void AutoPositionChildNodes( ImVec2 nodePos, float verticalSpacing = 380.0f );
            void SerialiseIncludingDependancies( struct ImGuiSettingsHandler* handler, struct ImGuiTextBuffer* buffer, std::unordered_set<int>& serialisedNodeIds );

            static constexpr int AttributeBitCount = 8;
            static constexpr int AttributeBitMask = ( 1 << AttributeBitCount ) - 1;

            static int GetNodeIdFromAttribute( int attributeId )
            {
                return (int)((unsigned int)attributeId >> AttributeBitCount);
            }

            int GetStartingAttributeId() const 
            {
                return nodeId << AttributeBitCount;
            }

            int GetOutputAttributeId() const
            {
                return GetStartingAttributeId() | AttributeBitMask;
            }

            FastNoiseNodeEditor& editor;
            std::unique_ptr<FastNoise::NodeData> data;
            std::string serialised;
            const int nodeId;

            int64_t totalGenerateNs = 0;
            std::vector<int64_t> generateAverages;

            static const int NoiseSize = 224;
            GL::Texture2D noiseTexture;
        };

        struct MetadataMenu
        {
            virtual ~MetadataMenu() = default;
            virtual const char* GetName() const = 0;
            virtual bool CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid = nullptr ) const = 0;
            virtual const FastNoise::Metadata* DrawUI( std::function<bool( const FastNoise::Metadata* )> isValid = nullptr, bool drawGroups = true ) const = 0;
        };

        struct MetadataMenuItem : MetadataMenu
        {
            MetadataMenuItem( const FastNoise::Metadata* metadata ) : metadata( metadata ) {}

            const char* GetName() const final { return metadata->name; }
            bool CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const final;
            const FastNoise::Metadata* DrawUI( std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const final;

            const FastNoise::Metadata* metadata;
        };

        struct MetadataMenuGroup : MetadataMenu
        {
            MetadataMenuGroup( const char* name ) : name( name ) {}

            const char* GetName() const final { return name; }
            bool CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const final;
            const FastNoise::Metadata* DrawUI( std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const final;

            const char* name;
            std::vector<const MetadataMenu*> items;
        };

        Node& AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata, bool generatePreview = true );
        bool AddNodeFromEncodedString( const char* string, ImVec2 nodePos );
        std::string_view GetSelectedEncodedNodeTree();
        FastNoise::OutputMinMax GenerateNodePreviewNoise( FastNoise::Generator* gen, float* noise );
        Node* FindNodeFromId( int id );
        int GetFreeNodeId();
        void SetPreviewGenerator( std::string_view encodedNodeTree, bool force = false );
        void ChangeSelectedNode( FastNoise::NodeData* newId );
        void DeleteNode( FastNoise::NodeData* nodeData );
        void DoNodeBenchmarks();
        void SetupSettingsHandlers();
        void OpenStandaloneNodeGraph();

        void CheckLinks();
        void DoHelp();
        void DoContextMenu();
        void DoNodes();
        void UpdateSelected();

        NodeEditorApp* mNodeEditorApp;

        std::unordered_map<FastNoise::NodeData*, Node> mNodes;
        FastNoise::NodeData* mDroppedLinkNode = nullptr;
        bool mDroppedLink = false;

        ImVec2 mContextStartPos;
        std::vector<std::unique_ptr<MetadataMenu>> mContextMetadata;
        std::string mImportNodeString;
        bool mImportNodeModal = false;
        bool mSettingsDirty = false;

        MeshNoisePreview mMeshNoisePreview;
        NoiseTexture mNoiseTexture;

        std::string mCachedSelectedEnt;
        FastNoise::NodeData* mSelectedNode = nullptr;
        Node mOverheadNode;
        int32_t mNodeBenchmarkIndex = 0;
        int32_t mNodeBenchmarkMax = 128;

        float mNodeScale = 2.5f;
        int mNodeSeed = 1337;
        NoiseTexture::GenType mNodeGenType = NoiseTexture::GenType_2D;

        FastSIMD::FeatureSet mMaxFeatureSet    = FastSIMD::FeatureSet::Max;
        FastSIMD::FeatureSet mActualFeatureSet = FastSIMD::FeatureSet::Invalid;
    };
}
