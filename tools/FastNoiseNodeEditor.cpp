#include <sstream>
#include <random>
#include <cstdio>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imnodes.h>

#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include "ImGuiExtra.h"
#include "FastNoiseNodeEditor.h"
#include "DemoNodeTrees.inl"
#include "NodeEditorApp.h"

using namespace Magnum;

#include "SharedMemoryIpc.inl"

static constexpr const char* kNodeGraphSettingsFile = "NodeGraph.ini";

void FastNoiseNodeEditor::OpenStandaloneNodeGraph()
{
#ifdef WIN32
    std::string startArgs = "\"";
    startArgs += mNodeEditorApp.GetExecutablePath();
    startArgs += "\" detached";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    // Create a job object
    HANDLE hJob = CreateJobObject( NULL, NULL );
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };

    // Configure the job object to terminate processes when the handle is closed
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject( hJob, JobObjectExtendedLimitInformation, &jeli, sizeof( jeli ) );

    // Start the child process.
    if( CreateProcessA( NULL, // No module name (use command line)
                         (LPSTR)startArgs.data(), // Command line
                         NULL, // Process handle not inheritable
                         NULL, // Thread handle not inheritable
                         FALSE, // Set handle inheritance to FALSE
                         0, // No creation flags
                         NULL, // Use parent's environment block
                         NULL, // Use parent's starting directory
                         &si, // Pointer to STARTUPINFO structure
                         &pi ) ) // Pointer to PROCESS_INFORMATION structure
    {
        // Assign the child process to the job object
        AssignProcessToJobObject( hJob, pi.hProcess );

        // Close handles to the child process and primary thread
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    else
#else
    pid_t pid = fork(); // Duplicate current process

    if( pid == 0 )
    {
        // Child process
        const char* executable = mNodeEditorApp.GetExecutablePath().data(); // Path to the current executable
        execl( executable, executable, "detached", (char*)NULL );
        // If execl returns, it means it has failed
        exit( EXIT_FAILURE ); // Ensure the child process exits if execl fails
    }
    if( pid < 0 )
#endif
    {
        Debug {} << "Failed to launch standalone node graph process"
#ifdef WIN32
            << GetLastError()
#endif
        ;
    }
}

static bool MatchingGroup( const std::vector<const char*>& a, const std::vector<const char*>& b )
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
static bool MatchingMembers( const std::vector<T>& a, const std::vector<T>& b )
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

static std::string TimeWithUnits( int64_t time, int significantDigits = 3 )
{
    if( time == 0 )
    {
        return "0us";
    }

    double f = time / 1e+3;
          
    int d = (int)::ceil(::log10(::abs( f ))); /*digits before decimal point*/
    std::stringstream ss;
    ss.precision( std::max(significantDigits - d, 0) );
    ss << std::fixed << f << "us";
    return ss.str();
}

template<size_t N, typename... Args>
std::string string_format( const char (&format)[N], const Args&... args )
{
    int size_s = std::snprintf( nullptr, 0, format, args... );
    if( size_s <= 0 )
    {
        return "";
    }
    auto size = static_cast<size_t>( size_s + 1 );
    std::string buf( size, 0 );
    std::snprintf( buf.data(), size, format, args... );
    return buf;
}

const char* string_format( const char* txt )
{
    return txt;
}

template<typename T, typename... Args>
static bool DoHoverPopup( T&& format, const Args&... args )
{
    if( ImGui::IsItemHovered() )
    {
        auto hoverTxt = string_format( format, args... );

        if( !hoverTxt[0] )
        {
            return false;
        }

        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
        ImGui::BeginTooltip();
        ImGui::TextUnformatted( &hoverTxt[0] );
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
        return true;
    }
    return false;
}

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, FastNoise::NodeData* nodeData, bool generatePreview, int id ) :
    Node( e, std::unique_ptr<FastNoise::NodeData>( nodeData ), generatePreview, id )
{ }

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, std::unique_ptr<FastNoise::NodeData>&& nodeData, bool generatePreview, int id ) :
    editor( e ),
    data( std::move( nodeData ) ),
    nodeId( id ? id : e.GetFreeNodeId() )
{
    assert( !e.FindNodeFromId( id ) );

    if( generatePreview )
    {
        GeneratePreview();
    }
}

void FastNoiseNodeEditor::Node::GeneratePreview( bool nodeTreeChanged, bool benchmark )
{
    static std::array<float, NoiseSize * NoiseSize> noiseData;

    serialised = FastNoise::Metadata::SerialiseNodeData( data.get(), true );
    auto generator = FastNoise::NewFromEncodedNodeTree( serialised.c_str(), editor.mMaxFeatureSet );

    if( !benchmark && nodeTreeChanged )
    {
        generateAverages.clear();
    }

    if( generator )
    {
        auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( editor.mMaxFeatureSet );
        auto scale = FastNoise::New<FastNoise::DomainScale>( editor.mMaxFeatureSet );
        genRGB->SetSource( scale );
        scale->SetSource( generator );
        scale->SetScaling( editor.mNodeScale );

        FastNoise::SmartNode<FastNoise::ConvertRGBA8> l(nullptr);
        
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
    
    for( auto& node : editor.mNodes )
    {
        for( FastNoise::NodeData* link : node.second.GetNodeIDLinks() )
        {
            if( link == data.get() )
            {
                node.second.GeneratePreview( nodeTreeChanged );
            }
        }
    }

    if( nodeTreeChanged )
    {
        if( editor.mSelectedNode == data.get() )
        {
            editor.ChangeSelectedNode( data.get() );
        }

        // Save nodes to ini
        editor.mSettingsDirty = true;
    }
}


std::vector<FastNoise::NodeData*> FastNoiseNodeEditor::Node::GetNodeIDLinks()
{
    std::vector<FastNoise::NodeData*> links;
    links.reserve( data->nodeLookups.size() + data->hybrids.size() );

    for( FastNoise::NodeData* link : data->nodeLookups )
    {
        links.emplace_back( link );
    }

    for( auto& link : data->hybrids )
    {
        links.emplace_back( link.first );
    }

    assert( links.size() < 16 );

    return links;
}

int64_t FastNoiseNodeEditor::Node::GetLocalGenerateNs()
{
    int64_t localTotal = totalGenerateNs;

    for( FastNoise::NodeData* link : GetNodeIDLinks() )
    {
        auto find = editor.mNodes.find( link );

        if( find != editor.mNodes.end() )
        {
            localTotal -= find->second.totalGenerateNs;
        }           
    }

    return std::max<int64_t>( localTotal, 0 );
}

FastNoise::NodeData*& FastNoiseNodeEditor::Node::GetNodeLink( int attributeId )
{
    attributeId &= 15;

    if( attributeId < (int)data->nodeLookups.size() )
    {
        return data->nodeLookups[attributeId];
    }
    else
    {
        attributeId -= (int)data->nodeLookups.size();
        return data->hybrids[attributeId].first;
    }
}

void FastNoiseNodeEditor::Node::AutoPositionChildNodes( ImVec2 nodePos, float verticalSpacing )
{
    auto nodeLinks = GetNodeIDLinks();
    nodeLinks.erase( std::remove( nodeLinks.begin(), nodeLinks.end(), nullptr ), nodeLinks.end() );

    if( nodeLinks.empty() )
    {
        return;
    }

    ImVec2 nodeSpacing = { 280, verticalSpacing };

    nodePos.x -= nodeSpacing.x;
    nodePos.y -= nodeSpacing.y * 0.5f * (nodeLinks.size() - 1);

    for( FastNoise::NodeData* link : nodeLinks )
    {
        ImNodes::SetNodeScreenSpacePos( editor.mNodes.at( link ).nodeId, nodePos );

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

void FastNoiseNodeEditor::Node::SerialiseIncludingDependancies( ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer, std::unordered_set<int>& serialisedNodeIds )
{
    if( serialisedNodeIds.find( nodeId ) != serialisedNodeIds.end() )
    {
        return;
    }

    for( FastNoise::NodeData* nodeData : GetNodeIDLinks() )
    {
        if( nodeData )
        {
            editor.mNodes.at( nodeData ).SerialiseIncludingDependancies( handler, buffer, serialisedNodeIds );
        }
    }

    buffer->appendf( "\n[%s][Node:%d]\n", handler->TypeName, data->metadata->id );


    for( const auto& var: data->variables )
    {
        buffer->appendf( "variable=%d\n", var.i );
    }
    for( const auto& node: data->nodeLookups )
    {
        buffer->appendf( "node=%d\n", node ? editor.mNodes.at( node ).nodeId : 0 );
    }
    for( const auto& hybrid: data->hybrids )
    {
        buffer->appendf( "hybrid=%i:%f\n", hybrid.first ? editor.mNodes.at( hybrid.first ).nodeId : 0, hybrid.second );        
    }

    // id must be after setting all members, it verifies and creates the node
    buffer->appendf( "id=%i\n", nodeId );

    // Must be after node creation
    ImVec2 gridPos = ImNodes::GetNodeGridSpacePos( nodeId );
    buffer->appendf( "grid_pos=%f:%f\n", gridPos.x, gridPos.y );

    serialisedNodeIds.emplace( nodeId );
}

void FastNoiseNodeEditor::SetupSettingsHandlers()
{
    ImGuiSettingsHandler nodeSettings;
    nodeSettings.TypeName = "NodeEditorNodeData";
    nodeSettings.TypeHash = ImHashStr( nodeSettings.TypeName );
    nodeSettings.UserData = this;
    nodeSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf )
    {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;

        std::unordered_set<int> serialisedNodeIds;

        // Save all root nodes
        for( auto& node: nodeEditor->mNodes )
        {
            bool hasReference = false;

            for( auto& linkNode: nodeEditor->mNodes )
            {
                auto links = linkNode.second.GetNodeIDLinks();

                if( std::find( links.begin(), links.end(), node.first ) != links.end() )
                {
                    hasReference = true;
                    break;
                }
            }

            if( !hasReference )
            {
                node.second.SerialiseIncludingDependancies( handler, outBuf, serialisedNodeIds );
            }
        }
    };
    nodeSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void*
    {
        int metadataId;
        if( sscanf( name, "Node:%d", &metadataId ) == 1 )
        {
            if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( metadataId ) )
            {
                FastNoise::NodeData* nodeData = new FastNoise::NodeData( metadata );
                nodeData->nodeLookups.clear();
                nodeData->variables.clear();
                nodeData->hybrids.clear();
                return nodeData;
            }
        }

        return nullptr;
    };
    nodeSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line )
    {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        auto* nodeData = (FastNoise::NodeData*)entry;

        ImVec2 imVec2;
        float f;
        int i;
        if( sscanf( line, "grid_pos=%f:%f", &imVec2.x, &imVec2.y ) == 2 )
        {
            auto find = nodeEditor->mNodes.find( nodeData );
            if( find != nodeEditor->mNodes.end() )
            {
                ImNodes::SetNodeGridSpacePos( find->second.nodeId, imVec2 );
            }
        }
        else if( sscanf( line, "variable=%d", &i ) == 1 )
        {
            nodeData->variables.push_back( i );
        }
        else if( sscanf( line, "node=%d", &i ) == 1 )
        {
            Node* link = nodeEditor->FindNodeFromId( i );

            nodeData->nodeLookups.push_back( link ? link->data.get() : nullptr );            
        }
        else if( sscanf( line, "hybrid=%d:%f", &i, &f ) == 2 )
        {
            Node* link = nodeEditor->FindNodeFromId( i );

            nodeData->hybrids.emplace_back( link ? link->data.get() : nullptr, f );            
        }
        else if( sscanf( line, "id=%d", &i ) == 1 )
        {
            // Check the data is valid (node class may have changed)
            if( nodeData->variables.size() == nodeData->metadata->memberVariables.size() &&
                nodeData->nodeLookups.size() == nodeData->metadata->memberNodeLookups.size() &&
                nodeData->hybrids.size() == nodeData->metadata->memberHybrids.size() )
            {
                if( !nodeEditor->FindNodeFromId( i ) && nodeEditor->mNodes.try_emplace( nodeData, *nodeEditor, nodeData, true, i ).second )
                {
                    return;
                }
            }

            delete nodeData;            
        }
    };


    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName = "NodeEditorNodeGraph";
    editorSettings.TypeHash = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf )
    {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        ImVec2 gridOffset = ImNodes::EditorContextGetPanning();
        outBuf->appendf( "grid_offset=%f:%f\n", gridOffset.x, gridOffset.y );

        outBuf->appendf( "scale=%f\n", nodeEditor->mNodeScale );
        outBuf->appendf( "seed=%d\n", nodeEditor->mNodeSeed );
        outBuf->appendf( "gen_type=%d\n", (int)nodeEditor->mNodeGenType );
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void*
    {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line )
    {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;

        ImVec2 imVec2;
        if( sscanf( line, "grid_offset=%f:%f", &imVec2.x, &imVec2.y ) == 2 )
        {
            ImNodes::EditorContextResetPanning( imVec2 );
        }

        sscanf( line, "scale=%f", &nodeEditor->mNodeScale );
        sscanf( line, "seed=%d", &nodeEditor->mNodeSeed );
        sscanf( line, "gen_type=%d", (int*)&nodeEditor->mNodeGenType );
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
    ImGuiExtra::AddOrReplaceSettingsHandler( nodeSettings );
}

FastNoiseNodeEditor::FastNoiseNodeEditor( NodeEditorApp& nodeEditorApp ) :
    mNodeEditorApp( nodeEditorApp ),
    mOverheadNode( *this, new FastNoise::NodeData( &FastNoise::Metadata::Get<FastNoise::Constant>() ), false )
{
    if( !mNodeEditorApp.IsDetachedNodeGraph() )
    {
#ifdef IMGUI_HAS_DOCK
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
        ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
    }
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImNodes::CreateContext();
    ImNodes::GetIO().AltMouseButton = ImGuiMouseButton_Right;

    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    ImNodes::GetStyle().MiniMapPadding = ImVec2( 8, 8 );

#ifndef NDEBUG
    mNodeBenchmarkMax = 1;
#endif

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
}

FastNoiseNodeEditor::~FastNoiseNodeEditor()
{
    // Go into node graph context and trigger save
    ImGuiContext* currentContext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext( ImNodes::GetNodeEditorImGuiContext() );
    ImGui::SaveIniSettingsToDisk( kNodeGraphSettingsFile );
    ImGui::SetCurrentContext( currentContext );

    ImNodes::DestroyContext();
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
    DoIpcPolling();

    bool isDetachedNodeEditor = mNodeEditorApp.IsDetachedNodeGraph();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGuiWindowFlags windowFlags = 0;
    ImGuiWindow* nodeGraphWindow = ImGui::FindWindowByName( "Node Graph" );

    if( isDetachedNodeEditor )
    {
        ImGui::SetNextWindowSize( viewport->WorkSize );
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
    }
    else if( nodeGraphWindow && nodeGraphWindow->Collapsed )
    {
        // Avoid saving over the window position when it is minimised from detach
        windowFlags = ImGuiWindowFlags_NoSavedSettings;
    }
    else
    {
        ImGui::DockSpaceOverViewport( viewport, ImGuiDockNodeFlags_PassthruCentralNode );     
        
        std::string simdTxt = "Current Feature Set: ";
        simdTxt += FastSIMD::GetFeatureSetString( mActualFeatureSet );
        ImGui::TextUnformatted( simdTxt.c_str() );

        ImGui::DragInt( "Node Benchmark Count", &mNodeBenchmarkMax, 8, 8, 64 * 1024 );

        ImGui::SetNextWindowSize( ImVec2( 963, 634 ), ImGuiCond_FirstUseEver );
        ImGui::SetNextWindowPos( ImVec2( 8, 439 ), ImGuiCond_FirstUseEver );
    }

    if( ImGui::Begin( "Node Graph", nullptr, windowFlags ) )
    {
        UpdateSelected();

        bool edited = false;
        ImGui::PushItemWidth( 82.0f );
        
        edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mNodeGenType ), NoiseTexture::GenTypeStrings );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mNodeGenType ), NoiseTexture::GenType_Count ); 
        ImGui::SameLine();  

        edited |= ImGui::DragInt( "Seed", &mNodeSeed );
        ImGui::SameLine();
        edited |= ImGui::DragFloat( "Scale", &mNodeScale, 0.05f );    
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

        bool openStandalonenodeGraph = false;
        if( !isDetachedNodeEditor )
        {
            ImGui::SameLine();
            if( ImGui::Button( "Detach Node Graph" ) )
            {
                openStandalonenodeGraph = true;

                ImGui::SetWindowCollapsed( true );
                ImGui::GetCurrentWindow()->Pos = ImVec2( 0, 0 );
            }
        }

        ImGui::PopItemWidth();
        
        if( edited )
        {
            for( auto& node : mNodes )
            {
                node.second.GeneratePreview( false );
            }

            mSettingsDirty = true;
        }

        ImNodes::BeginNodeEditor();

        // Setup setting handles in zoom context
        if( ImGui::GetFrameCount() == 1 )
        {
            SetupSettingsHandlers();
            ImGui::LoadIniSettingsFromDisk( kNodeGraphSettingsFile );
        }
        if( mSettingsDirty )
        {
            ImGui::MarkIniSettingsDirty();
            mSettingsDirty = false;
        }
        if( ImGui::GetIO().WantSaveIniSettings || openStandalonenodeGraph )
        {
            ImGui::SaveIniSettingsToDisk( kNodeGraphSettingsFile );
            ImGui::GetIO().WantSaveIniSettings = false;
        }

        // Open this after saving settings
        if( openStandalonenodeGraph )
        {
            OpenStandaloneNodeGraph();
        }

        DoHelp();

        DoContextMenu();

        DoNodes();

        ImNodes::MiniMap( 0.2f, ImNodesMiniMapLocation_BottomLeft );

        ImNodes::EndNodeEditor();

        // Zoom
        if( ImNodes::IsEditorHovered() && ImGui::GetIO().MouseWheel != 0 )
        {
            float zoom = ImNodes::EditorContextGetZoom();
            if( ImGui::GetIO().MouseWheel > 0 )
            {
                zoom *= 1.5f;
                if( zoom > 0.9f )
                {
                    zoom = 1;
                }
            }
            else
            {
                zoom /= 1.5f;
                zoom = std::max( zoom, 0.2f );
            }
            ImNodes::EditorContextSetZoom( zoom, ImGui::GetMousePos() );
        }

        CheckLinks();

    }
    ImGui::End();

    DoNodeBenchmarks();

    if( !isDetachedNodeEditor )
    {
        mNoiseTexture.Draw();

        mMeshNoisePreview.Draw( transformation, projection, cameraPosition );
    }
}

void FastNoiseNodeEditor::CheckLinks()
{
    // Check for new links
    int startNodeId, endNodeId;
    int startAttr, endAttr;
    bool createdFromSnap;
    if( ImNodes::IsLinkCreated( &startNodeId, &startAttr, &endNodeId, &endAttr, &createdFromSnap ) )
    {
        Node* startNode = FindNodeFromId( startNodeId );
        Node* endNode = FindNodeFromId( endNodeId );

        if( startNode && endNode )
        {
            auto& link = endNode->GetNodeLink( endAttr );

            if( !createdFromSnap || !link )
            {
                link = startNode->data.get();
                endNode->GeneratePreview();
            }
        }
    }

    int attrStart;
    if( ImNodes::IsLinkDropped( &attrStart, false ) )
    {
        Node* node = FindNodeFromId( Node::GetNodeIdFromAttribute( attrStart ) );

        if( node && ( attrStart & Node::AttributeBitMask ) == Node::AttributeBitMask )
        {
            mDroppedLinkNode = node->data.get();
            mDroppedLink = true;
        }
    }
}

void FastNoiseNodeEditor::DeleteNode( FastNoise::NodeData* nodeData )
{
    mNodes.erase( nodeData );

    for( auto& node : mNodes )
    {
        bool changed = false;
        int attrId = node.second.GetStartingAttributeId();

        for( FastNoise::NodeData* link : node.second.GetNodeIDLinks() )
        {
            if( link == nodeData )
            {
                node.second.GetNodeLink( attrId ) = nullptr;
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

void FastNoiseNodeEditor::UpdateSelected()
{
    std::vector<int> linksToDelete;
    int selectedLinkCount = ImNodes::NumSelectedLinks();

    bool delKeyPressed = !ImGui::GetIO().WantTextInput && (
        ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) ||
        ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Backspace ), false ) );

    if( selectedLinkCount && delKeyPressed )
    {
        linksToDelete.resize( selectedLinkCount );
        ImNodes::GetSelectedLinks( linksToDelete.data() );
    }

    int destroyedLinkId;
    if( ImNodes::IsLinkDestroyed( &destroyedLinkId ) )
    {
        linksToDelete.push_back( destroyedLinkId );
    }

    for( int deleteID : linksToDelete )
    {
        for( auto& node : mNodes )
        {
            bool changed = false;
            int attributeId = node.second.GetStartingAttributeId();

            for( FastNoise::NodeData* link : node.second.GetNodeIDLinks() )
            {
                (void)link;
                if( attributeId == deleteID )
                {
                    node.second.GetNodeLink( attributeId ) = nullptr;
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

    int selectedNodeCount = ImNodes::NumSelectedNodes();

    if( selectedNodeCount && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
    {
        std::vector<int> selected( selectedNodeCount );

        ImNodes::GetSelectedNodes( selected.data() );

        for( int deleteID: selected )
        {
            if( Node* node = FindNodeFromId( deleteID ) )
            {
                DeleteNode( node->data.get() );
            }
        }
    }
}

void FastNoiseNodeEditor::SetSIMDLevel( FastSIMD::FeatureSet lvl )
{
    mMaxFeatureSet = lvl;

    mOverheadNode.generateAverages.clear();
    DoNodeBenchmarks();

    for( auto& node : mNodes )
    {
        node.second.generateAverages.clear();
        node.second.GeneratePreview( false );
    }

    SetPreviewGenerator( mCachedActiveEnt );
}

void FastNoiseNodeEditor::DoNodes()
{
    for( auto& node : mNodes )
    {
        ImNodes::BeginNode( node.second.nodeId );

        ImNodes::BeginNodeTitleBar();
        std::string formatName = FastNoise::Metadata::FormatMetadataNodeName( node.second.data->metadata );
        ImGui::TextUnformatted( formatName.c_str() );

#ifdef NDEBUG
        DoHoverPopup( "%s", node.first->metadata->description );
#else
        DoHoverPopup( "%s : %d\n%s", node.first->metadata->name, node.first->metadata->id, node.first->metadata->description );
#endif
        std::string performanceString = TimeWithUnits( node.second.GetLocalGenerateNs() );

        ImGui::SameLine( Node::NoiseSize - ImGui::CalcTextSize( performanceString.c_str() ).x );
        ImGui::TextUnformatted( performanceString.c_str() );

        if( ImGui::IsItemHovered() )
        {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
            ImGui::SetTooltip( "Total: %s", TimeWithUnits( node.second.totalGenerateNs ).c_str() );            
            ImGui::PopStyleVar();
        }

        ImNodes::EndNodeTitleBar();

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

                    for( FastNoise::NodeData* link : node.second.data->nodeLookups )
                    {
                        links.emplace( link );
                    }
                    for( auto& link : node.second.data->hybrids )
                    {
                        links.emplace( link.first );
                    }

                    for( auto& link : newData.nodeLookups )
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

        ImNodes::PushAttributeFlag( ImNodesAttributeFlags_EnableLinkCreationOnSnap );
        ImNodes::PushAttributeFlag( ImNodesAttributeFlags_EnableLinkDetachWithDragClick );
        int attributeId = node.second.GetStartingAttributeId();
        auto& nodeMetadata = node.second.data->metadata;
        auto& nodeData = node.second.data;

        for( auto& memberNode : nodeMetadata->memberNodeLookups )
        {
            ImNodes::BeginInputAttribute( attributeId++ );
            formatName = FastNoise::Metadata::FormatMetadataMemberName( memberNode );
            ImGui::TextUnformatted( formatName.c_str() );
            ImNodes::EndInputAttribute();
            
            DoHoverPopup( memberNode.description );
        }

        for( size_t i = 0; i < node.second.data->metadata->memberHybrids.size(); i++ )
        {
            ImNodes::BeginInputAttribute( attributeId++ );

            bool isLinked = node.second.data->hybrids[i].first;
            const char* floatFormat = "%.3f";

            if( isLinked )
            {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                floatFormat = "";
            }

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeMetadata->memberHybrids[i] );

            if( ImGui::DragFloat( formatName.c_str(), &nodeData->hybrids[i].second, nodeMetadata->memberHybrids[i].valueUiDragSpeed, 0, 0, floatFormat ) )
            {
                node.second.GeneratePreview();
            }

            if( isLinked )
            {
                ImGui::PopItemFlag();
            }
            ImNodes::EndInputAttribute();
            DoHoverPopup( nodeMetadata->memberHybrids[i].description );
        }

        for( size_t i = 0; i < nodeMetadata->memberVariables.size(); i++ )
        {
            ImNodes::BeginStaticAttribute( 0 );

            auto& nodeVar = nodeMetadata->memberVariables[i];

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeVar );

            switch( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
            {
                if( ImGui::DragFloat( formatName.c_str(), &nodeData->variables[i].f, nodeVar.valueUiDragSpeed, nodeVar.valueMin.f, nodeVar.valueMax.f ) )
                {
                    node.second.GeneratePreview();
                }
            }
            break;
            case FastNoise::Metadata::MemberVariable::EInt:
            {
                if( ImGui::DragInt( formatName.c_str(), &nodeData->variables[i].i, nodeVar.valueUiDragSpeed, nodeVar.valueMin.i, nodeVar.valueMax.i ) )
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

            ImNodes::EndStaticAttribute();
            DoHoverPopup( nodeMetadata->memberVariables[i].description );
        }

        ImGui::PopItemWidth();
        ImNodes::PopAttributeFlag();
        ImNodes::BeginOutputAttribute( node.second.GetOutputAttributeId(), ImNodesPinShape_QuadFilled );

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
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    // Do current node links
    for( auto& node : mNodes )
    {
        int attributeId = node.second.GetStartingAttributeId();

        for( FastNoise::NodeData* link : node.second.GetNodeIDLinks() )
        {
            if( link )
            {
                ImNodes::Link( attributeId, mNodes.at( link ).GetOutputAttributeId(), attributeId );
            }
            attributeId++;
        }
    }
}

FastNoiseNodeEditor::Node& FastNoiseNodeEditor::AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata, bool generatePreview )
{
    FastNoise::NodeData* nodeData = new FastNoise::NodeData( metadata );
    
    auto newNode = mNodes.try_emplace( nodeData, *this, nodeData, generatePreview );

    ImNodes::SetNodeScreenSpacePos( newNode.first->second.nodeId, startPos );

    if( mNodes.size() == 1 )
    {
        ChangeSelectedNode( nodeData );
    }

    return newNode.first->second;
}

bool FastNoiseNodeEditor::AddNodeFromEncodedString( const char* string, ImVec2 nodePos )
{
    std::vector<std::unique_ptr<FastNoise::NodeData>> nodeData;

    if( FastNoise::NodeData* firstNodeData = FastNoise::Metadata::DeserialiseNodeData( string, nodeData ) )
    {
        for( auto& data : nodeData )
        {
            FastNoise::NodeData* newNodeData = data.get();
            mNodes.emplace( std::piecewise_construct, std::forward_as_tuple( newNodeData ), std::forward_as_tuple( *this, std::move( data ) ) );
        }

        if( mNodes.size() == nodeData.size() )
        {
            ChangeSelectedNode( firstNodeData );
        }

        Node& firstNode = mNodes.at( firstNodeData );

        ImNodes::SetNodeScreenSpacePos( firstNode.nodeId, nodePos );
        firstNode.AutoPositionChildNodes( nodePos );
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

        ImGui::TextUnformatted( "Add nodes" );
        ImGui::SameLine( alignPx );
        ImGui::TextUnformatted( "Right mouse click" );

        ImGui::TextUnformatted( "Pan graph" );
        ImGui::SameLine( alignPx );
        ImGui::TextUnformatted( "Right mouse drag" );

        ImGui::TextUnformatted( "Zoom graph" );
        ImGui::SameLine( alignPx );
        ImGui::TextUnformatted( "Mouse wheel" );

        ImGui::TextUnformatted( "Delete node/link" );
        ImGui::SameLine( alignPx );
        ImGui::TextUnformatted( "Backspace or Delete" );

        ImGui::TextUnformatted( "Node options" );
        ImGui::SameLine( alignPx );
        ImGui::TextUnformatted( "Right click node title" );

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

            newNode.GetNodeLink( 0 ) = mDroppedLinkNode;
            newNode.GeneratePreview();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

std::string_view FastNoiseNodeEditor::GetSelectedEncodedNodeTree()
{
    auto find = mNodes.find( mSelectedNode );

    if( find != mNodes.end() )
    {
        return find->second.serialised;
    }

    return { "" };
}

FastNoise::OutputMinMax FastNoiseNodeEditor::GenerateNodePreviewNoise( FastNoise::Generator* gen, float* noise )
{
    switch( mNodeGenType )
    {
    case NoiseTexture::GenType_2D:
        return gen->GenUniformGrid2D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2,
            Node::NoiseSize, Node::NoiseSize, mNodeSeed );

    case NoiseTexture::GenType_2DTiled:
        return gen->GenTileable2D( noise,
            Node::NoiseSize, Node::NoiseSize, mNodeSeed );

    case NoiseTexture::GenType_3D:
        return gen->GenUniformGrid3D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2, 0,
            Node::NoiseSize, Node::NoiseSize, 1, mNodeSeed );

    case NoiseTexture::GenType_4D:
        return gen->GenUniformGrid4D( noise,
            Node::NoiseSize / -2, Node::NoiseSize / -2, 0, 0,
            Node::NoiseSize, Node::NoiseSize, 1, 1, mNodeSeed );

    case NoiseTexture::GenType_Count:
        break;
    }

    return {};
}

FastNoiseNodeEditor::Node* FastNoiseNodeEditor::FindNodeFromId( int id )
{
    auto find = std::find_if( mNodes.begin(), mNodes.end(), [id]( const auto& node ) 
    {
        return node.second.nodeId == id;
    } );

    if( find != mNodes.end() )
    {
        return &find->second;
    }

    return nullptr;
}

int FastNoiseNodeEditor::GetFreeNodeId()
{
    static int newNodeId = 0;

    do
    {
        newNodeId = std::max( 1, ( newNodeId + 1 ) & ( INT_MAX >> Node::AttributeBitCount ) );

    } while( FindNodeFromId( newNodeId ) );

    return newNodeId;
}

void FastNoiseNodeEditor::ChangeSelectedNode( FastNoise::NodeData* newId )
{
    mSelectedNode = newId;

    std::string_view encodedNodeTree = GetSelectedEncodedNodeTree();

    if( !encodedNodeTree.empty() )
    {
        // Send updated node tree via IPC
        unsigned char* sharedMemory = static_cast<unsigned char*>( mNodeEditorApp.GetIpcSharedMemory() );

        if( encodedNodeTree.length() + 3 >= kSharedMemorySize )
        {
            Debug {} << "Encoded node tree too large to send via IPC " << encodedNodeTree.length();
            sharedMemory = nullptr;
        }

        if( sharedMemory )
        {
            memcpy( sharedMemory + 2, encodedNodeTree.data(), encodedNodeTree.length() + 1 );
            sharedMemory[1] = 0;

            std::atomic_thread_fence( std::memory_order_acq_rel );
            sharedMemory[0]++; // Increment counter to mark updated tree
        }
        else
        {
            SetPreviewGenerator( encodedNodeTree );
        }
    }
}

void FastNoiseNodeEditor::SetPreviewGenerator( std::string_view encodedNodeTree )
{
    auto SetActiveEnt = [this]( std::string_view encodedNodeTree )
    {
        if( GetSelectedEncodedNodeTree() != encodedNodeTree )
        {
            mSelectedNode = nullptr;
        }

        mCachedActiveEnt = encodedNodeTree;
    };

    FastNoise::SmartNode<> generator = FastNoise::NewFromEncodedNodeTree( encodedNodeTree.data(), mMaxFeatureSet );

    if( generator )
    {
        mActualFeatureSet = generator->GetActiveFeatureSet();

        if( !mNodeEditorApp.IsDetachedNodeGraph() )
        {
            mNoiseTexture.ReGenerate( generator );
            mMeshNoisePreview.ReGenerate( generator );
        }

        if( !encodedNodeTree.empty() )
        {
            SetActiveEnt( encodedNodeTree );
        }
    }
    else if( encodedNodeTree.empty() )
    {
        SetActiveEnt( encodedNodeTree );
    }
    else
    {
        Debug {} << "Invalid encoded node tree";
    }
}

