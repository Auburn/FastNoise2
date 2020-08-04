#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>

#include "FastNoiseNodeEditor.h"

#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include "ImGuiExtra.h"

using namespace Magnum;

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, FastNoise::NodeData* nodeData, const FastNoise::Metadata* metadata ) :
    editor( e ),
    data( nodeData )
{
    data->metadata = metadata;            

    for( auto& value : metadata->memberVariables )
    {
        data->variables.push_back( value.valueDefault );
    }

    for( auto& value : metadata->memberNodes )
    {
        data->nodes.push_back( nullptr );
    }

    for( auto& value : metadata->memberHybrids )
    {
        data->hybrids.emplace_back( nullptr, value.valueDefault );
    }
    GeneratePreview();
}

void FastNoiseNodeEditor::Node::GeneratePreview( bool nodeTreeChanged )
{
    static std::array<float, NoiseSize * NoiseSize> noiseData;

    serialised = FastNoise::MetadataManager::SerialiseNodeData( data.get(), true );
    auto generator = FastNoise::NewFromEncodedNodeTree( serialised.c_str() );

    if( generator )
    {
        auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( editor.mMaxSIMDLevel );
        genRGB->SetSource( generator );

        float frequency = editor.mNodeFrequency;

        genRGB->GenUniformGrid2D( noiseData.data(), NoiseSize / -2, NoiseSize / -2, NoiseSize, NoiseSize, frequency, editor.mNodeSeed );
    }
    else
    {
        std::fill( noiseData.begin(), noiseData.end(), 0.0f );
        serialised.clear();
    }

    ImageView2D noiseImage( PixelFormat::RGBA8Unorm, { NoiseSize, NoiseSize }, noiseData );
    
    noiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, noiseImage.size() )
        .setSubImage( 0, {}, noiseImage );

    int myNodeId = GetNodeID();

    for( auto& node : editor.mNodes )
    {
        for( int link : node.second.GetNodeIDLinks() )
        {
            if( link == myNodeId )
            {
                node.second.GeneratePreview( nodeTreeChanged );
            }
        }
    }

    if( editor.mSelectedNode == myNodeId && nodeTreeChanged )
    {
        editor.ChangeSelectedNode( myNodeId );
    }
}


std::vector<int> FastNoiseNodeEditor::Node::GetNodeIDLinks()
{
    std::vector<int> links;

    for( const FastNoise::NodeData* link : data->nodes )
    {
        links.emplace_back( GetNodeID( link ) );
    }

    for( auto& link : data->hybrids )
    {
        links.emplace_back( GetNodeID( link.first ) );
    }
    
    return links;
}

void FastNoiseNodeEditor::Node::SetNodeLink( int attributeId, FastNoise::NodeData* nodeData )
{
    attributeId &= 15;
    
    if( attributeId < (int)data->nodes.size() )
    {
        data->nodes[attributeId] = nodeData;            
    }
    else
    {
        attributeId -= (int)data->nodes.size();
        data->hybrids[attributeId].first = nodeData;
    }
}

FastNoiseNodeEditor::FastNoiseNodeEditor()
{
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
                node.second.GeneratePreview( false );
            }
        }

        std::string simdTxt = "Current SIMD Level: ";
        simdTxt += GetSIMDLevelName( mActualSIMDLevel );
        ImGui::SameLine( ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize( simdTxt.c_str() ).x );
        ImGui::TextUnformatted( simdTxt.c_str() );

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
        for( auto& node : mNodes )
        {
            int attrId = node.second.GetStartingAttributeID();

            for( int link : node.second.GetNodeIDLinks() )
            {
                int linkId = 0;
                if( attrId == startAttr )
                {
                    linkId = endAttr;
                }
                else if( attrId == endAttr )
                {
                    linkId = startAttr;
                }

                if( linkId )
                {
                    if( createdFromSnap && link )
                    {
                        break;
                    }
                    node.second.SetNodeLink( attrId, mNodes.at( Node::GetNodeID( linkId ) ).data.get() );
                    node.second.GeneratePreview();
                    break;
                }
                attrId++;
            }
        }
    }

    int linkStart;
    if( imnodes::IsLinkDropped( &linkStart, false ) )
    {
        int nodeId = Node::GetNodeID( linkStart );

        auto find = mNodes.find( nodeId );
        if( find != mNodes.end() && (linkStart & 15) == 15 )
        {
            mDroppedLinkNodeId = find->second.data.get();
            mDroppedLink = true;
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
            int attributeId = node.second.GetStartingAttributeID();

            for( int link : node.second.GetNodeIDLinks() )
            {
                if( attributeId == deleteID )
                {
                    node.second.SetNodeLink( attributeId, nullptr );
                    changed = true;
                }
                attributeId++;
            }

            if( changed )
            {
                node.second.GeneratePreview();
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
                int attrId = node.second.GetStartingAttributeID();

                for( int link : node.second.GetNodeIDLinks() )
                {
                    if( link == deleteID )
                    {
                        node.second.SetNodeLink( attrId, nullptr );
                        changed = true;
                    }
                    attrId++;
                }

                if( changed )
                {
                    node.second.GeneratePreview();
                }
            }
        }        
    }
}

void FastNoiseNodeEditor::SetSIMDLevel( FastSIMD::eLevel lvl )
{
    mMaxSIMDLevel = lvl;

    for( auto& node : mNodes )
    {
        node.second.GeneratePreview();
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

void FormatMemberName( std::string& string, const char* name, int dimension )
{
    string = name;
    if( dimension >= 0 )
    {
        const char dimensionNames[] = { 'X', 'Y', 'Z', 'W' };

        string.insert( 0, { dimensionNames[dimension], ' ' } );
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
    std::string formatName;

    for( auto& node : mNodes )
    {
        imnodes::BeginNode( node.first );

        imnodes::BeginNodeTitleBar();
        FormatClassName( formatName, node.second.data->metadata->name );
        ImGui::TextUnformatted( formatName.c_str() );
        imnodes::EndNodeTitleBar();

        // Right click node title to change node type
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
        if( ImGui::BeginPopupContextItem() )
        {
            for( auto metadata : FastNoise::MetadataManager::GetMetadataClasses() )
            {
                auto& nodeMetadata = node.second.data->metadata;

                if( metadata == nodeMetadata ||
                    !MatchingMembers( metadata->memberVariables, nodeMetadata->memberVariables ) ||
                    !MatchingMembers( metadata->memberNodes, nodeMetadata->memberNodes ) ||
                    !MatchingMembers( metadata->memberHybrids, nodeMetadata->memberHybrids ) )
                {
                    continue;
                }

                FormatClassName( formatName, metadata->name );
                if( ImGui::MenuItem( formatName.c_str() ) )
                {
                    nodeMetadata = metadata;
                    node.second.GeneratePreview();
                }
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImGui::PushItemWidth( 60.0f );        

        imnodes::PushAttributeFlag( imnodes::AttributeFlags_EnableLinkCreationOnSnap );
        imnodes::PushAttributeFlag( imnodes::AttributeFlags_EnableLinkDetachWithDragClick );
        int attributeId = node.second.GetStartingAttributeID();
        auto& nodeMetadata = node.second.data->metadata;
        auto& nodeData = node.second.data;

        for( auto& memberNode : nodeMetadata->memberNodes )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            FormatMemberName( formatName, memberNode.name, memberNode.dimensionIdx );
            ImGui::TextUnformatted( formatName.c_str() );
            imnodes::EndInputAttribute();
        }

        for( size_t i = 0; i < node.second.data->metadata->memberHybrids.size(); i++ )
        {
            imnodes::BeginInputAttribute( attributeId++ );

            bool isLinked = node.second.data->hybrids[i].first;
            const char* floatFormat = "%.3f";

            if( isLinked )
            {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                floatFormat = "";
            }

            FormatMemberName( formatName, nodeMetadata->memberHybrids[i].name, nodeMetadata->memberHybrids[i].dimensionIdx );

            if( ImGui::DragFloat( formatName.c_str(), &nodeData->hybrids[i].second, 0.02f, 0, 0, floatFormat ) )
            {
                node.second.GeneratePreview();
            }

            if( isLinked )
            {
                ImGui::PopItemFlag();
            }
            imnodes::EndInputAttribute();
        }

        for( size_t i = 0; i < nodeMetadata->memberVariables.size(); i++ )
        {
            auto& nodeVar = nodeMetadata->memberVariables[i];

            FormatMemberName( formatName, nodeVar.name, nodeVar.dimensionIdx );

            switch ( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
                {
                    if( ImGui::DragFloat( formatName.c_str(), &nodeData->variables[i].f, 0.02f, nodeVar.valueMin.f, nodeVar.valueMax.f ) )
                    {
                        node.second.GeneratePreview();
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EInt:
                {
                    if( ImGui::DragInt( formatName.c_str(), &nodeData->variables[i].i, 0.2f, nodeVar.valueMin.i, nodeVar.valueMax.i ) )
                    {
                        node.second.GeneratePreview();
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EEnum:
                {
                    if( ImGui::Combo( formatName.c_str(), &nodeData->variables[i].i, nodeVar.enumNames.data(), (int)nodeVar.enumNames.size() ) ||
                        ImGuiExtra::ScrollCombo( &nodeData->variables[i].i, (int)nodeVar.enumNames.size() ) )
                    {
                        node.second.GeneratePreview();
                    }
                }
                break;
            }
        }

        ImGui::PopItemWidth();
        imnodes::PopAttributeFlag();
        imnodes::BeginOutputAttribute( Node::GetOutputAttributeId( node.second.GetNodeID() ), imnodes::PinShape_QuadFilled );

        Vector2 noiseSize = { (float)Node::NoiseSize, (float)Node::NoiseSize };
        if( mSelectedNode == node.first && !node.second.serialised.empty() )
        {
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImGui::RenderFrame( cursorPos - ImVec2( 1, 1 ), cursorPos + ImVec2( noiseSize ) + ImVec2( 1, 1 ), IM_COL32( 255, 0, 0, 200 ), false );
        }
        ImGuiIntegration::image( node.second.noiseTexture, noiseSize );

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
        int attributeId = node.second.GetStartingAttributeID();

        for( int link : node.second.GetNodeIDLinks() )
        {
            if( link )
            {
                imnodes::Link( attributeId, Node::GetOutputAttributeId( link ), attributeId );
            }
            attributeId++;
        }
    }
}

FastNoiseNodeEditor::Node& FastNoiseNodeEditor::AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata )
{
    FastNoise::NodeData* nodeData = new FastNoise::NodeData();
    
    int newNodeId = Node::GetNodeID( nodeData );
    auto& newNode = mNodes.emplace( std::piecewise_construct, std::forward_as_tuple( newNodeId ), std::forward_as_tuple( *this, nodeData, metadata ) );

    imnodes::SetNodeScreenSpacePos( newNodeId, startPos );

    if( mNodes.size() == 1 )
    {
        ChangeSelectedNode( newNodeId );
    }

    return newNode.first->second;
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
                auto& newNode = AddNode( startPos, metadata );

                newNode.SetNodeLink( 0, mDroppedLinkNodeId );
                newNode.GeneratePreview();
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

FastNoise::SmartNode<> FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = mNodes.find( mSelectedNode );

    FastNoise::SmartNode<> generator;
    const char* serialised = "";

    if( find != mNodes.end() )
    {
        serialised = find->second.serialised.c_str();
        generator = FastNoise::NewFromEncodedNodeTree( serialised, mMaxSIMDLevel );

        if( generator )
        {
            mActualSIMDLevel = generator->GetSIMDLevel();
        }
    }

    mNoiseTexture.ReGenerate( generator, serialised );

    return generator;
}

void FastNoiseNodeEditor::ChangeSelectedNode( int newId )
{
    mSelectedNode = newId;

    FastNoise::SmartNode<> generator = GenerateSelectedPreview();

    if( generator )
    {
        mMeshNoisePreview.ReGenerate( generator );
    }
}

const char* FastNoiseNodeEditor::GetSIMDLevelName( FastSIMD::eLevel lvl )
{
    switch( lvl )
    {
    default:
    case FastSIMD::Level_Null:   return "NULL";
    case FastSIMD::Level_Scalar: return "Scalar";
    case FastSIMD::Level_SSE:    return "SSE";
    case FastSIMD::Level_SSE2:   return "SSE2";
    case FastSIMD::Level_SSE3:   return "SSE3";
    case FastSIMD::Level_SSSE3:  return "SSSE3";
    case FastSIMD::Level_SSE41:  return "SSE4.1";
    case FastSIMD::Level_SSE42:  return "SSE4.2";
    case FastSIMD::Level_AVX:    return "AVX";
    case FastSIMD::Level_AVX2:   return "AVX2";
    case FastSIMD::Level_AVX512: return "AVX512";
    case FastSIMD::Level_NEON:   return "NEON";
    }
}
