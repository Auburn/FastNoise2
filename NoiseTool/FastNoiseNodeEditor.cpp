#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imnodes.h>
#include <sstream>
#include <random>

#include "FastNoiseNodeEditor.h"

#include "DemoNodeTrees.inl"

#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include "ImGuiExtra.h"

using namespace Magnum;

bool MatchingGroup( const std::vector<const char*>& a, const std::vector<const char*>& b )
{
    std::string aString;
    for( const char* c : a )
    {
        aString.append( c );
        aString.push_back( '\t' );
    }

    std::string bString;
    for( const char* c : b )
    {
        bString.append( c );
        bString.push_back( '\t' );
    }

    return aString == bString;
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

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e ) :
    editor( e )
{
}

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, FastNoise::NodeData* nodeData ) :
    editor( e ),
    data( nodeData )
{
    GeneratePreview();
}

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, std::unique_ptr<FastNoise::NodeData>&& nodeData ) :
    editor( e ),
    data( std::move( nodeData ) )
{
    GeneratePreview();
}

void FastNoiseNodeEditor::Node::GeneratePreview( bool nodeTreeChanged, bool benchmark )
{
    static std::array<float, NoiseSize * NoiseSize> noiseData;

    serialised = FastNoise::Metadata::SerialiseNodeData( data.get(), true );
    auto generator = FastNoise::NewFromEncodedNodeTree( serialised.c_str(), editor.mMaxSIMDLevel );

    if( !benchmark && nodeTreeChanged )
    {
        generateAverages.clear();
    }

    if( generator )
    {
        auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( editor.mMaxSIMDLevel );
        genRGB->SetSource( generator );
        
        auto startTime = std::chrono::high_resolution_clock::now();

        editor.GenerateNodePreviewNoise( genRGB.get(), noiseData.data() );

        generateAverages.push_back( std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::high_resolution_clock::now() - startTime ).count() - editor.mOverheadNode.totalGenerateNs );

        std::sort( generateAverages.begin(), generateAverages.end() );

        totalGenerateNs = generateAverages[generateAverages.size() / 3];
    }
    else
    {
        std::fill( noiseData.begin(), noiseData.end(), 0.0f );
        serialised.clear();
        totalGenerateNs = 0;
    }

    if( benchmark )
    {
        return;        
    }

    ImageView2D noiseImage( PixelFormat::RGBA8Unorm, { NoiseSize, NoiseSize }, noiseData );

    noiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, noiseImage.size() ).setSubImage( 0, {}, noiseImage );

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

    assert( links.size() < 16 );

    return links;
}

uint64_t FastNoiseNodeEditor::Node::GetLocalGenerateNs()
{
    int64_t localTotal = totalGenerateNs;

    for( int link : GetNodeIDLinks() )
    {
        auto find = editor.mNodes.find( link );

        if( find != editor.mNodes.end() )
        {
            localTotal -= find->second.totalGenerateNs;
        }           
    }

    return std::max<int64_t>( localTotal, 0 );
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

void FastNoiseNodeEditor::Node::AutoPositionChildNodes( ImVec2 nodePos, float verticalSpacing )
{
    auto nodeLinks = GetNodeIDLinks();
    nodeLinks.erase( std::remove( nodeLinks.begin(), nodeLinks.end(), 0 ), nodeLinks.end() );

    if( nodeLinks.empty() )
    {
        return;
    }

    ImVec2 nodeSpacing = { 280, verticalSpacing };

    nodePos.x -= nodeSpacing.x;
    nodePos.y -= nodeSpacing.y * 0.5f * (nodeLinks.size() - 1);

    for( int link : nodeLinks )
    {
        imnodes::SetNodeScreenSpacePos( link, nodePos );

        editor.mNodes.at( link ).AutoPositionChildNodes( nodePos, nodeLinks.size() > 1 ? verticalSpacing * 0.6f : verticalSpacing );
        nodePos.y += nodeSpacing.y;
    }
}

bool FastNoiseNodeEditor::MetadataMenuItem::CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const
{
    return !isValid || isValid( metadata );
}

const FastNoise::Metadata* FastNoiseNodeEditor::MetadataMenuItem::DrawUI( std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const
{
    std::string format = FastNoise::Metadata::FormatMetadataNodeName( metadata, true );
    
    if( ImGui::MenuItem( format.c_str() ) )
    {
        return metadata;
    }
    return nullptr;
}

bool FastNoiseNodeEditor::MetadataMenuGroup::CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const
{
    for( const auto& item : items )
    {
        if( item->CanDraw( isValid ) )
        {
            return true;
        }
    }
    return false;
}

const FastNoise::Metadata* FastNoiseNodeEditor::MetadataMenuGroup::DrawUI( std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const
{
    const FastNoise::Metadata* returnPressed = nullptr;

    bool doGroup = drawGroups && name[0] != 0;

    if( !doGroup || ImGui::BeginMenu( name ) )
    {
        for( const auto& item : items )
        {
            if( item->CanDraw( isValid ) )
            {
                if( auto pressed = item->DrawUI( isValid, drawGroups ) )
                {
                    returnPressed = pressed;
                }
            }
        }
        if( doGroup )
        {
            ImGui::EndMenu();
        }
    }
    return returnPressed;
}

FastNoiseNodeEditor::FastNoiseNodeEditor() :
    mOverheadNode( *this )
{
    ImGui::CreateContext();

#ifdef IMGUI_HAS_DOCK
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGui::GetIO().ConfigWindowsResizeFromEdges = true;

    imnodes::CreateContext();
    imnodes::GetIO().alt_mouse_button = ImGuiMouseButton_Right;

    ImGui::StyleColorsDark();
    imnodes::StyleColorsDark();

    // Create Metadata context menu tree
    std::unordered_map<std::string, MetadataMenuGroup*> groupMap;
    auto* root = new MetadataMenuGroup( "" );
    mContextMetadata.emplace_back( root );

    auto menuSort = []( const MetadataMenu* a, const MetadataMenu* b ) { return std::strcmp( a->GetName(), b->GetName() ) < 0; };

    for( const FastNoise::Metadata* metadata : FastNoise::Metadata::GetAll() )
    {
        auto* metaDataGroup = root;

        std::string groupTree;

        for( const char* group : metadata->groups )
        {
            groupTree += group;
            auto find = groupMap.find( groupTree );
            if( find == groupMap.end() )
            {
                auto* newGroup = new MetadataMenuGroup( group );
                mContextMetadata.emplace_back( newGroup );
                metaDataGroup->items.emplace_back( newGroup );
                find = groupMap.emplace( groupTree, newGroup ).first;

                std::sort( metaDataGroup->items.begin(), metaDataGroup->items.end(), menuSort );
            }

            metaDataGroup = find->second;
            groupTree += '\t';
        }

        metaDataGroup->items.emplace_back( mContextMetadata.emplace_back( new MetadataMenuItem( metadata ) ).get() );
        std::sort( metaDataGroup->items.begin(), metaDataGroup->items.end(), menuSort );
    }
    
    mOverheadNode.data = std::make_unique<FastNoise::NodeData>( &FastNoise::Metadata::Get<FastNoise::Constant>() );
}

void FastNoiseNodeEditor::DoNodeBenchmarks()
{
    // Benchmark overhead every frame to keep it accurate
    if( mOverheadNode.generateAverages.size() >= 512 )
    {
        std::default_random_engine engine( (uint32_t)mOverheadNode.totalGenerateNs );
        std::uniform_int_distribution<size_t> randomInt { 0ul, mOverheadNode.generateAverages.size() - 1 };

        mOverheadNode.generateAverages.erase( mOverheadNode.generateAverages.begin() + randomInt( engine ) );    
    }

    mOverheadNode.GeneratePreview( false, true );

    // 1 node benchmark per frame
    if( mNodeBenchmarkIndex >= (int32_t)mNodes.size() )
    {
        mNodeBenchmarkIndex = 0;
    }

    for( auto itr = std::next( mNodes.begin(), mNodeBenchmarkIndex ); itr != mNodes.end(); ++itr )
    {
        mNodeBenchmarkIndex++;
        if( !itr->second.serialised.empty() && itr->second.generateAverages.size() < mNodeBenchmarkMax )
        {
            itr->second.GeneratePreview( false, true );
            break;
        }
    }
}

void FastNoiseNodeEditor::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition )
{
    std::string simdTxt = "Current SIMD Level: ";
    simdTxt += GetSIMDLevelName( mActualSIMDLevel );
    ImGui::TextUnformatted( simdTxt.c_str() );

    ImGui::DragInt( "Node Benchmark Count", &mNodeBenchmarkMax, 8, 8, 64 * 1024 );

    ImGui::SetNextWindowSize( ImVec2( 963, 634 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 8, 439 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Node Editor" ) )
    {
        UpdateSelected();

        bool edited = false;
        ImGui::PushItemWidth( 82.0f );

        edited |= ImGui::DragInt( "Seed", &mNodeSeed );
        ImGui::SameLine();
        edited |= ImGui::DragFloat( "Frequency", &mNodeFrequency, 0.001f );    
        ImGui::SameLine();    
        
        edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mNodeGenType ), NoiseTexture::GenTypeStrings );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mNodeGenType ), NoiseTexture::GenType_Count );
        ImGui::SameLine();   

        if( ImGui::Button( "Retest Node Performance" ) )
        {
            for( auto& node : mNodes )
            {
                node.second.generateAverages.clear();
            }
        }
        if( ImGui::IsItemHovered() )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( "Disable \"Generate Mesh Preview\" for more accurate results" );
            ImGui::EndTooltip();
        }

        ImGui::PopItemWidth();
        
        if( edited )
        {
            for( auto& node : mNodes )
            {
                node.second.GeneratePreview( false );
            }
        }                

        imnodes::BeginNodeEditor();
        
        DoHelp();

        DoContextMenu();

        DoNodes();

        imnodes::EndNodeEditor();

        CheckLinks();

    }
    ImGui::End();

    DoNodeBenchmarks();

    mNoiseTexture.Draw();

    mMeshNoisePreview.Draw( transformation, projection, cameraPosition );
}

void FastNoiseNodeEditor::CheckLinks()
{
    // Check for new links
    int startAttr, endAttr;
    bool createdFromSnap;
    if( imnodes::IsLinkCreated( &startAttr, &endAttr, &createdFromSnap ) )
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
    int selectedLinkCount = imnodes::NumSelectedLinks();

    bool delKeyPressed =
        ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) ||
        ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Backspace ), false );

    if( selectedLinkCount && delKeyPressed )
    {
        linksToDelete.resize( selectedLinkCount );
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
                (void)link;
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

    int selectedNodeCount = imnodes::NumSelectedNodes();

    if( selectedNodeCount && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
    {
        std::vector<int> selected( selectedNodeCount );

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

    mOverheadNode.generateAverages.clear();
    DoNodeBenchmarks();

    for( auto& node : mNodes )
    {
        node.second.generateAverages.clear();
        node.second.GeneratePreview( false );
    }

    ChangeSelectedNode( mSelectedNode );
}

void FastNoiseNodeEditor::DoNodes()
{
    for( auto& node : mNodes )
    {
        imnodes::BeginNode( node.first );

        imnodes::BeginNodeTitleBar();
        std::string formatName = FastNoise::Metadata::FormatMetadataNodeName( node.second.data->metadata );
        ImGui::TextUnformatted( formatName.c_str() );

        std::stringstream performanceStream;
        performanceStream.precision( 3 );
        performanceStream << node.second.GetLocalGenerateNs() / 1e+3f << "us";
        std::string performanceString = performanceStream.str();

        ImGui::SameLine( Node::NoiseSize - ImGui::CalcTextSize( performanceString.c_str() ).x );
        ImGui::Text( performanceString.c_str() );

        if( ImGui::IsItemHovered() )
        {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
            ImGui::BeginTooltip();
            ImGui::Text( "Total: %.3gus", node.second.totalGenerateNs / 1e+3f );
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }

        imnodes::EndNodeTitleBar();

        // Right click node title to change node type
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
        if( ImGui::BeginPopupContextItem() )
        {
            if( ImGui::MenuItem( "Copy Encoded Node Tree" ) )
            {
                ImGui::SetClipboardText( node.second.serialised.c_str() );
                Debug{} << node.second.serialised.c_str();
            }

            ImGui::Separator();
            ImGui::MenuItem( "Convert To:", nullptr, nullptr, false );

            auto& nodeMetadata = node.second.data->metadata;
            auto newMetadata = mContextMetadata.front()->DrawUI( [nodeMetadata]( const FastNoise::Metadata* metadata )
            {
                return metadata != nodeMetadata && MatchingGroup( metadata->groups, nodeMetadata->groups );
            },  false );

            if( newMetadata )
            {
                if( MatchingMembers( newMetadata->memberVariables, nodeMetadata->memberVariables ) &&
                    MatchingMembers( newMetadata->memberNodeLookups, nodeMetadata->memberNodeLookups ) &&
                    MatchingMembers( newMetadata->memberHybrids, nodeMetadata->memberHybrids ) )
                {
                    nodeMetadata = newMetadata;                    
                }
                else
                {
                    FastNoise::NodeData newData( newMetadata );

                    std::queue<FastNoise::NodeData*> links;

                    for( FastNoise::NodeData* link : node.second.data->nodes )
                    {
                        links.emplace( link );
                    }
                    for( auto& link : node.second.data->hybrids )
                    {
                        links.emplace( link.first );
                    }

                    for( auto& link : newData.nodes )
                    {
                        if( links.empty() ) break;
                        link = links.front();
                        links.pop();
                    }
                    for( auto& link : newData.hybrids )
                    {
                        if( links.empty() ) break;
                        link.first = links.front();
                        links.pop();
                    }

                    *node.second.data = std::move( newData );                  
                }

                node.second.GeneratePreview();
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

        for( auto& memberNode : nodeMetadata->memberNodeLookups )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            formatName = FastNoise::Metadata::FormatMetadataMemberName( memberNode );
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

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeMetadata->memberHybrids[i] );

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
            imnodes::BeginStaticAttribute( 0 );

            auto& nodeVar = nodeMetadata->memberVariables[i];

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeVar );

            switch( nodeVar.type )
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

            imnodes::EndStaticAttribute();
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
    FastNoise::NodeData* nodeData = new FastNoise::NodeData( metadata );

    int newNodeId = Node::GetNodeID( nodeData );
    auto newNode = mNodes.emplace( std::piecewise_construct, std::forward_as_tuple( newNodeId ), std::forward_as_tuple( *this, nodeData ) );

    imnodes::SetNodeScreenSpacePos( newNodeId, startPos );

    if( mNodes.size() == 1 )
    {
        ChangeSelectedNode( newNodeId );
    }

    return newNode.first->second;
}

bool FastNoiseNodeEditor::AddNodeFromEncodedString( const char* string, ImVec2 nodePos )
{
    std::vector<std::unique_ptr<FastNoise::NodeData>> nodeData;

    if( FastNoise::NodeData* firstNode = FastNoise::Metadata::DeserialiseNodeData( string, nodeData ) )
    {
        int firstNodeId = Node::GetNodeID( firstNode );

        for( auto& data : nodeData )
        {
            int newNodeId = Node::GetNodeID( data.get() );
            mNodes.emplace( std::piecewise_construct, std::forward_as_tuple( newNodeId ), std::forward_as_tuple( *this, std::move( data ) ) );
        }

        if( mNodes.size() == nodeData.size() )
        {
            ChangeSelectedNode( firstNodeId );
        }

        imnodes::SetNodeScreenSpacePos( firstNodeId, nodePos );
        mNodes.at( firstNodeId ).AutoPositionChildNodes( nodePos );
        return true;
    }

    return false;
}

void FastNoiseNodeEditor::DoHelp()
{
    ImGui::Text( " Help" );
    if( ImGui::IsItemHovered() )
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
        ImGui::BeginTooltip();
        constexpr float alignPx = 110;

        ImGui::Text( "Add nodes" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right mouse click" );

        ImGui::Text( "Pan graph" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right mouse drag" );

        ImGui::Text( "Delete node/link" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Backspace or Delete" );

        ImGui::Text( "Node options" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right click node title" );

        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

void FastNoiseNodeEditor::DoContextMenu()
{
    std::string className;
    ImVec2 drag = ImGui::GetMouseDragDelta( ImGuiMouseButton_Right );
    float distance = sqrtf( ImDot( drag, drag ) );
    bool openImportModal = false;

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
    if( distance < 5.0f && ImGui::BeginPopupContextWindow( "new_node", 1 ) )
    {
        mContextStartPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        if( auto newMetadata = mContextMetadata.front()->DrawUI() )
        {
            AddNode( mContextStartPos, newMetadata );
        }

        if( ImGui::BeginMenu( "Import" ) )
        {
            if( ImGui::MenuItem( "Encoded Node Tree" ) )
            {
                openImportModal = true;
            }
            ImGui::Separator();
            for( size_t i = 0; i < sizeof( gDemoNodeTrees ) / sizeof( gDemoNodeTrees[0] ); i++ )
            {
                if( ImGui::MenuItem( gDemoNodeTrees[i][0] ) )
                {
                    AddNodeFromEncodedString( gDemoNodeTrees[i][1], mContextStartPos );
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    if( openImportModal )
    {
        mImportNodeModal = true;
        mImportNodeString.clear();
        ImGui::OpenPopup( "New From Encoded Node Tree" );
    }

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 5,5 } );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 5,5 } );
    ImGui::SetNextWindowSize( { 400, 92 }, ImGuiCond_Always );
    ImGui::SetNextWindowPos( ImGui::GetIO().DisplaySize / 2, ImGuiCond_Always, { 0.5f,0.5f } );

    if( ImGui::BeginPopupModal( "New From Encoded Node Tree", &mImportNodeModal, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {
        if( openImportModal )
        {
            ImGui::SetKeyboardFocusHere();
        }

        bool txtEnter = ImGui::InputText( "Base64 String", &mImportNodeString, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank );

        if( txtEnter | ImGui::Button( "Create", { 100, 30 } ) )
        {
            if( AddNodeFromEncodedString( mImportNodeString.c_str(), mContextStartPos ) )
            {
                mImportNodeModal = false;
            }
            else
            {
                mImportNodeString = "DESERIALISATION FAILED";
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar( 2 );

    if( mDroppedLink )
    {
        ImGui::OpenPopup( "new_node_drop" );
        mDroppedLink = false;
    }
    if( ImGui::BeginPopup( "new_node_drop", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings ) )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        auto newMetadata = mContextMetadata.front()->DrawUI( []( const FastNoise::Metadata* metadata )
        {
            return !metadata->memberNodeLookups.empty() || !metadata->memberHybrids.empty();
        } );

        if( newMetadata )
        {
            auto& newNode = AddNode( startPos, newMetadata );

            newNode.SetNodeLink( 0, mDroppedLinkNodeId );
            newNode.GeneratePreview();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

FastNoise::SmartNode<> FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = mNodes.find( mSelectedNode );

    FastNoise::SmartNode<> generator;

    if( find != mNodes.end() )
    {
        generator = FastNoise::NewFromEncodedNodeTree( find->second.serialised.c_str(), mMaxSIMDLevel );

        if( generator )
        {
            mActualSIMDLevel = generator->GetSIMDLevel();
        }
    }

    mNoiseTexture.ReGenerate( generator );

    return generator;
}

FastNoise::OutputMinMax FastNoiseNodeEditor::GenerateNodePreviewNoise( FastNoise::Generator* gen, float* noise )
{
    switch( mNodeGenType )
    {
    case NoiseTexture::GenType_2D:
        return gen->GenUniformGrid2D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2,
            Node::NoiseSize, Node::NoiseSize,
            mNodeFrequency, mNodeSeed );

    case NoiseTexture::GenType_2DTiled:
        return gen->GenTileable2D( noise,
            Node::NoiseSize, Node::NoiseSize,
            mNodeFrequency, mNodeSeed );

    case NoiseTexture::GenType_3D:
        return gen->GenUniformGrid3D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2, 0,
            Node::NoiseSize, Node::NoiseSize, 1,
            mNodeFrequency, mNodeSeed );

    case NoiseTexture::GenType_4D:
        return gen->GenUniformGrid4D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2, 0, 0,
            Node::NoiseSize, Node::NoiseSize, 1, 1,
            mNodeFrequency, mNodeSeed );
    }

    return {};
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
