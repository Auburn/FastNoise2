#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>

#include "FastNoiseNodeEditor.h"

#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

using namespace Magnum;

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, const FastNoise::Metadata* meta ) :
    editor( e ),
    metadata( meta )
{

}

void FastNoiseNodeEditor::Node::GeneratePreview( bool nodeTreeChanged )
{
    static std::array<float, NoiseSize * NoiseSize> noiseData;

    std::unordered_set<int> dependancies;
    std::vector<std::unique_ptr<FastNoise::NodeData>> nodeDatas;
    auto generator = GetGenerator( dependancies, nodeDatas );

    if( generator )
    {
        auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>();
        genRGB->SetSource( generator );

        float frequency = editor.mNodeFrequency;

        genRGB->GenUniformGrid2D( noiseData.data(), 0, 0, NoiseSize, NoiseSize, frequency, editor.mNodeSeed );

        serialised = FastNoise::MetadataManager::SerialiseNodeData( nodeDatas.back().get() );
    }
    else
    {
        std::fill( noiseData.begin(), noiseData.end(), 0 );
        serialised.clear();
    }

    ImageView2D noiseImage( PixelFormat::RGBA8Unorm, { NoiseSize, NoiseSize }, noiseData );
    
    noiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, noiseImage.size() )
        .setSubImage( 0, {}, noiseImage );

    for( auto& node : editor.mNodes )
    {
        for( int* link : node.second->memberLinks )
        {
            if( *link >> 8 == id )
            {
                node.second->GeneratePreview( nodeTreeChanged );
            }
        }
    }

    if( editor.mSelectedNode == id && nodeTreeChanged )
    {
        editor.ChangeSelectedNode( id );
    }
}

std::shared_ptr<FastNoise::Generator> FastNoiseNodeEditor::Node::GetGenerator( std::unordered_set<int>& dependancies, std::vector<std::unique_ptr<FastNoise::NodeData>>& nodeDatas )
{
    std::unordered_map<int, Node::Ptr>& nodes = editor.mNodes;
    std::unique_ptr<FastNoise::NodeData> nodeData( new FastNoise::NodeData() );
    std::shared_ptr<FastNoise::Generator> node( metadata->NodeFactory() );
    dependancies.insert( id );

    for( int* link : memberLinks )
    {
        if( dependancies.find( *link >> 8 ) != dependancies.end() )
        {
            *link = -1;
        }
    }

    for( size_t i = 0; i < metadata->memberHybrids.size(); i++ )
    {
        FastNoise::NodeData* sourceData = nullptr;
        auto source = nodes.find( memberHybrids[i].first >> 8 );

        if( source != nodes.end() )
        {
            auto sourceGen = source->second->GetGenerator( dependancies, nodeDatas );
            if( sourceGen )
            {
                sourceData = nodeDatas.back().get();
            }
            else
            {
                return nullptr;
            }

            if( !metadata->memberHybrids[i].setNodeFunc( node.get(), sourceGen ) )
            {
                sourceData = nullptr;
                memberHybrids[i].first = -1;
            }
        }

        if( !sourceData )
        {
            metadata->memberHybrids[i].setValueFunc( node.get(), memberHybrids[i].second );
        }
        nodeData->hybrids.emplace_back( sourceData, memberHybrids[i].second );
    }

    for( size_t i = 0; i < metadata->memberNodes.size(); i++ )
    {
        auto source = nodes.find( memberNodes[i] >> 8 );

        if( source == nodes.end() )
        {
            memberNodes[i] = -1;
            return nullptr;
        }

        auto sourceGen = source->second->GetGenerator( dependancies, nodeDatas );
        if( sourceGen )
        {
            nodeData->nodes.push_back( nodeDatas.back().get() );                  
        }
        else
        {
            return nullptr;
        }

        if( !metadata->memberNodes[i].setFunc( node.get(), sourceGen ) )        
        {
            memberNodes[i] = -1;
            return nullptr;
        }        
    }

    for( size_t i = 0; i < metadata->memberVariables.size(); i++ )
    {
        metadata->memberVariables[i].setFunc( node.get(), memberValues[i] );
        nodeData->variables.push_back( memberValues[i] );
    }

    nodeData->metadata = metadata;
    nodeDatas.push_back( std::move( nodeData ) );
    return node;
}

FastNoiseNodeEditor::FastNoiseNodeEditor()
{
    std::string lSIMD = "FastSIMD detected SIMD Level: ";

    switch( FastSIMD::CPUMaxSIMDLevel() )
    {
    default:
    case FastSIMD::Level_Null:   lSIMD.append( "NULL" ); break;
    case FastSIMD::Level_Scalar: lSIMD.append( "Scalar" ); break;
    case FastSIMD::Level_SSE:    lSIMD.append( "SSE" ); break;
    case FastSIMD::Level_SSE2:   lSIMD.append( "SSE2" ); break;
    case FastSIMD::Level_SSE3:   lSIMD.append( "SSE3" ); break;
    case FastSIMD::Level_SSSE3:  lSIMD.append( "SSSE3" ); break;
    case FastSIMD::Level_SSE41:  lSIMD.append( "SSE4.1" ); break;
    case FastSIMD::Level_SSE42:  lSIMD.append( "SSE4.2" ); break;
    case FastSIMD::Level_AVX:    lSIMD.append( "AVX" ); break;
    case FastSIMD::Level_AVX2:   lSIMD.append( "AVX2" ); break;
    case FastSIMD::Level_AVX512: lSIMD.append( "AVX512" ); break;
    case FastSIMD::Level_NEON:   lSIMD.append( "NEON" ); break;
    }

    Debug{} << lSIMD.c_str();

#ifdef IMGUI_HAS_DOCK
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGui::GetIO().ConfigWindowsResizeFromEdges = true;

    imnodes::Initialize();

    ImGui::StyleColorsDark();
    imnodes::StyleColorsDark();
}

void FastNoiseNodeEditor::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition )
{
    ImGui::SetNextWindowSize( ImVec2( 963, 634 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 8, 439 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Node Editor" ) )
    {
        UpdateSelected();

        bool edited = false;
        ImGui::PushItemWidth( 60.0f );
        edited |= ImGui::DragInt( "Seed", &mNodeSeed );
        ImGui::SameLine();
        edited |= ImGui::DragFloat( "Frequency", &mNodeFrequency, 0.001f );
        ImGui::PopItemWidth();

        if( edited )
        {
            for( auto& node : mNodes )
            {
                node.second->GeneratePreview( false );
            }
        }

        imnodes::BeginNodeEditor();

        DoNodes();

        DoContextMenu();

        imnodes::EndNodeEditor();        

        CheckLinks();
    }
    ImGui::End();

    mNoiseTexture.Draw();

    mMeshNoisePreview.Draw( transformation, projection, cameraPosition );    
}

void FastNoiseNodeEditor::CheckLinks()
{
    // Check for new links
    int startAttr, endAttr;
    bool createdFromSnap;
    if( imnodes::IsLinkCreated( &startAttr, &endAttr, &createdFromSnap) )
    {
        for( auto& n : mNodes )
        {
            int attrId = n.first << 8;

            for( int* id : n.second->memberLinks )
            {
                int linkId = -1;
                if( attrId == startAttr )
                {
                    linkId = endAttr;
                }
                else if( attrId == endAttr )
                {
                    linkId = startAttr;
                }

                if( linkId != -1 )
                {
                    if( createdFromSnap && *id != -1 )
                    {
                        break;
                    }
                    *id = linkId;
                    n.second->GeneratePreview();
                    break;
                }
                attrId++;
            }
        }
    }

    int linkStart;
    if( imnodes::IsLinkDropped( &linkStart, false ) )
    {
        auto find = mNodes.find( linkStart >> 8 );
        if( find != mNodes.end() && linkStart - (find->first << 8) == (int)find->second->memberLinks.size() )
        {
            mDroppedLink = true;
            mDroppedLinkNodeId = linkStart;
        }
    }
}

void FastNoiseNodeEditor::UpdateSelected()
{
    std::vector<int> linksToDelete;

    if( int selectedCount = imnodes::NumSelectedLinks() && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
    {
        linksToDelete.resize( selectedCount );
        imnodes::GetSelectedLinks( linksToDelete.data() );
    }

    int destroyedLinkId;
    if( imnodes::IsLinkDestroyed( &destroyedLinkId ) )
    {
        linksToDelete.push_back( destroyedLinkId );
    }

    for( int deleteID : linksToDelete )
    {
        for( auto& node : mNodes )
        {
            bool changed = false;
            int attributeId = node.first << 8;

            for( int* link : node.second->memberLinks )
            {
                if( attributeId == deleteID )
                {
                    *link = -1;
                    changed = true;
                }
                attributeId++;
            }

            if( changed )
            {
                node.second->GeneratePreview();
            }
        }
    }

    if( int selectedCount = imnodes::NumSelectedNodes() && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
    {
        std::vector<int> selected( selectedCount );

        imnodes::GetSelectedNodes( selected.data() );

        for( int deleteID : selected )
        {
            mNodes.erase( deleteID );

            for( auto& node : mNodes )
            {
                bool changed = false;

                for( int* link : node.second->memberLinks )
                {
                    if( *link >> 8 == deleteID )
                    {
                        *link = -1;
                        changed = true;
                    }
                }

                if( changed )
                {
                    node.second->GeneratePreview();
                }
            }
        }        
    }
}

void FormatClassName( std::string& string, const char* name )
{
    string = name;
    for( int i = 1; i < string.size(); i++ )
    {
        if( isupper( string[i] ) && islower( string[i - 1] ) )
        {
            string.insert( i++, 1, ' ' );
        }
    }
}

template<typename T>
bool MatchingMembers( const std::vector<T>& a, const std::vector<T>& b )
{
    if( a.size() != b.size() )
    {
        return false;
    }

    for( size_t i = 0; i < a.size(); i++ )
    {
        if( strcmp( a[i].name, b[i].name ) != 0 )
        {
            return false;
        }
    }
    return true;
}

void FastNoiseNodeEditor::DoNodes()
{
    std::string className;

    for( auto& node : mNodes )
    {
        imnodes::BeginNode( node.first );

        imnodes::BeginNodeTitleBar();
        FormatClassName( className, node.second->metadata->name );
        ImGui::TextUnformatted( className.c_str() );
        imnodes::EndNodeTitleBar();

        // Right click node title to change node type
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
        if( ImGui::BeginPopupContextItem() )
        {
            for( auto& metadata : FastNoise::MetadataManager::GetMetadataClasses() )
            {
                if( metadata == node.second->metadata ||
                    !MatchingMembers( metadata->memberVariables, node.second->metadata->memberVariables ) ||
                    !MatchingMembers( metadata->memberNodes, node.second->metadata->memberNodes ) || 
                    !MatchingMembers( metadata->memberHybrids, node.second->metadata->memberHybrids ) )
                {
                    continue;
                }

                FormatClassName( className, metadata->name );
                if( ImGui::MenuItem( className.c_str() ) )
                {
                    node.second->metadata = metadata;
                    node.second->GeneratePreview();
                }
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImGui::PushItemWidth( 60.0f );        

        imnodes::PushAttributeFlag( imnodes::AttributeFlags_EnableLinkCreationOnSnap );
        imnodes::PushAttributeFlag( imnodes::AttributeFlags_EnableLinkDetachWithDragClick );
        int attributeId = node.first << 8;

        for( auto& memberNode : node.second->metadata->memberNodes )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            ImGui::TextUnformatted( memberNode.name );
            imnodes::EndInputAttribute();
        }

        for( size_t i = 0; i < node.second->metadata->memberHybrids.size(); i++ )
        {
            imnodes::BeginInputAttribute( attributeId++ );

            bool isLinked = node.second->memberHybrids[i].first != -1;
            const char* floatFormat = "%.3f";

            if( isLinked )
            {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                floatFormat = "";
            }

            if( ImGui::DragFloat( node.second->metadata->memberHybrids[i].name, &node.second->memberHybrids[i].second, 0.02f, 0, 0, floatFormat ) )
            {
                node.second->GeneratePreview();
            }

            if( isLinked )
            {
                ImGui::PopItemFlag();
            }
            imnodes::EndInputAttribute();
        }

        for( size_t i = 0; i < node.second->metadata->memberVariables.size(); i++ )
        {
            auto& nodeVar = node.second->metadata->memberVariables[i];

            switch ( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
                {
                    if( ImGui::DragFloat( nodeVar.name, &node.second->memberValues[i].f, 0.02f, nodeVar.valueMin.f, nodeVar.valueMax.f ) )
                    {
                        node.second->GeneratePreview();
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EInt:
                {
                    if( ImGui::DragInt( nodeVar.name, &node.second->memberValues[i].i, 0.2f, nodeVar.valueMin.i, nodeVar.valueMax.i ) )
                    {
                        node.second->GeneratePreview();
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EEnum:
                {
                    if( ImGui::Combo( nodeVar.name, &node.second->memberValues[i].i, nodeVar.enumNames.data(), (int)nodeVar.enumNames.size() ) )
                    {
                        node.second->GeneratePreview();
                    }
                }
                break;
            }
        }

        ImGui::PopItemWidth();
        imnodes::PopAttributeFlag();
        imnodes::BeginOutputAttribute( attributeId, imnodes::PinShape_QuadFilled );

        Vector2 noiseSize = { (float)Node::NoiseSize, (float)Node::NoiseSize };
        if( mSelectedNode == node.first && !node.second->serialised.empty() )
        {
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImGui::RenderFrame( cursorPos - ImVec2( 1, 1 ), cursorPos + ImVec2( noiseSize ) + ImVec2( 1, 1 ), IM_COL32( 255, 0, 0, 200 ), false );
        }
        ImGuiIntegration::image( node.second->noiseTexture, noiseSize );

        if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
        {
            ChangeSelectedNode( node.first );
        }
        imnodes::EndOutputAttribute();
        imnodes::PopAttributeFlag();

        imnodes::EndNode();
    }

    // Do current node links
    for( auto& node : mNodes )
    {
        int attributeId = node.first << 8;

        for( int* link : node.second->memberLinks )
        {
            if( *link != -1 )
            {
                imnodes::Link( attributeId, *link, attributeId );
            }
            attributeId++;
        }
    }
}

int FastNoiseNodeEditor::AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata )
{
    const int nodeId = ++mCurrentNodeId;
    imnodes::SetNodeScreenSpacePos( nodeId, startPos );

    mNodes.emplace( nodeId, new Node( *this, metadata ) );
                
    Node::Ptr& node = mNodes[nodeId];
    node->id = nodeId;

    for( auto& value : metadata->memberVariables )
    {
        node->memberValues.push_back( value.valueDefault );
    }

    node->memberNodes.reserve( metadata->memberNodes.size() );
    for( auto& value : metadata->memberNodes )
    {
        node->memberNodes.push_back( -1 );
        node->memberLinks.push_back( &node->memberNodes.back() );
    }

    node->memberHybrids.reserve( metadata->memberHybrids.size() );
    for( auto& value : metadata->memberHybrids )
    {
        node->memberHybrids.emplace_back( -1, value.valueDefault );
        node->memberLinks.push_back( &node->memberHybrids.back().first );
    }
    node->GeneratePreview();

    if( nodeId == 1 )
    {
        ChangeSelectedNode( nodeId );
    }

    return nodeId;
}

void FastNoiseNodeEditor::DoContextMenu()
{
    std::string className;
    ImVec2 drag = ImGui::GetMouseDragDelta( ImGuiMouseButton_Right );
    float distance = sqrtf( ImDot(drag, drag ) );

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2(4, 4) );
    if( distance < 5.0f && ImGui::BeginPopupContextWindow( "new_node", 1, false ) )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        for( auto& metadata : FastNoise::MetadataManager::GetMetadataClasses() )
        {
            FormatClassName( className, metadata->name );
            if( ImGui::MenuItem( className.c_str() ) )
            {
                AddNode( startPos, metadata );
            }
        }

        ImGui::EndPopup();
    }

    if( mDroppedLink )
    {
        ImGui::OpenPopup( "new_node_drop" );
        mDroppedLink = false;
    }
    if( ImGui::BeginPopup( "new_node_drop", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings ) )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        for( auto& metadata : FastNoise::MetadataManager::GetMetadataClasses() )
        {
            FormatClassName( className, metadata->name );
            if( (!metadata->memberNodes.empty() || !metadata->memberHybrids.empty()) && ImGui::MenuItem( className.c_str() ) )
            {
                auto& newNode = mNodes[AddNode( startPos, metadata )];

                *newNode->memberLinks[0] = mDroppedLinkNodeId;
                newNode->GeneratePreview();
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

std::shared_ptr<FastNoise::Generator> FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = mNodes.find( mSelectedNode );

    std::shared_ptr<FastNoise::Generator> generator;
    const char* serialised = "";

    if( find != mNodes.end() )
    {
        serialised = find->second->serialised.c_str();
        generator = FastNoise::NewFromEncodedNodeTree( serialised );
    }

    mNoiseTexture.ReGenerate( generator, serialised );

    return generator;
}

void FastNoiseNodeEditor::ChangeSelectedNode( int newId )
{
    mSelectedNode = newId;

    std::shared_ptr<FastNoise::Generator> generator = GenerateSelectedPreview();

    if( generator )
    {
        mMeshNoisePreview.ReGenerate( generator );
    }
}
