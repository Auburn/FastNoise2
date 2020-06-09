#include "FastNoiseNodeEditor.h"
#include "imnodes.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>


using namespace Magnum;

FastNoiseNodeEditor::Node::Node( const FastNoise::Metadata* meta ) :
    metadata( meta ),
    noiseImage( PixelFormat::RGBA8Srgb, { NoiseSize, NoiseSize } )
{

}

void FastNoiseNodeEditor::Node::GeneratePreview( FastNoiseNodeEditor* editor )
{
    auto gen = FastNoise::New<FastNoise::ConvertRGBA8>();

    bool isValid = true;
    std::vector<int> dependancies;
    gen->SetSource( GetGenerator( editor->mNodes, dependancies, isValid ) );

    if( isValid )
    {
        float frequency = editor->mNodeFrequency * 2.0f;
        float offset = frequency * NoiseSize * -0.5f;

        gen->GenUniformGrid2D( noiseData, offset, offset, NoiseSize, NoiseSize, frequency, frequency, editor->mNodeSeed );
    }
    else
    {
        memset( noiseData, 0, sizeof( noiseData ));
    }

    noiseImage.setData( noiseData );
    
    noiseTexture.setMagnificationFilter( GL::SamplerFilter::Linear )
        .setMinificationFilter( GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear )
        .setWrapping( GL::SamplerWrapping::ClampToEdge )
        .setMaxAnisotropy( GL::Sampler::maxMaxAnisotropy() )
        .setStorage( Math::log2( NoiseSize ) + 1, GL::TextureFormat::RGBA8, { NoiseSize, NoiseSize } )
        .setSubImage( 0, {}, noiseImage )
        .generateMipmap();

    for( auto& node : editor->mNodes )
    {
        for( int* link : node->memberLinks )
        {
            if( *link >> 8 == id )
            {
                node->GeneratePreview( editor );
            }
        }
    }

    if( editor->mSelectedNode == id )
    {
        editor->GenerateSelectedPreview();
    }
}

std::shared_ptr<FastNoise::Generator> FastNoiseNodeEditor::Node::GetGenerator( std::vector<Node::Ptr>& nodes, std::vector<int>& dependancies, bool& valid )
{
    std::shared_ptr<FastNoise::Generator> node( metadata->NodeFactory() );
    dependancies.push_back( id );

    for( int* link : memberLinks )
    {
        if( *link != -1 && std::find( dependancies.begin(), dependancies.end(), *link >> 8 ) != dependancies.end() )
        {
            *link = -1;
            valid = false;
        }        
    }

    for( int i = 0; i < metadata->memberNodes.size(); i++ )
    {
        auto source = std::find_if(nodes.begin(), nodes.end(),
            [id = memberNodes[i] >> 8]( const Node::Ptr& n ) { return n->id == id; });

        if( source == nodes.end() )
        {
            valid = false;
            continue;
        }

        auto sourceGen = source->get()->GetGenerator( nodes, dependancies, valid );
        if( !metadata->memberNodes[i].setFunc( node.get(), sourceGen ) )
        {
            memberNodes[i] = -1;
            valid = false;
        }        
    }

    for( int i = 0; i < metadata->memberHybrids.size(); i++ )
    {
        auto source = std::find_if(nodes.begin(), nodes.end(),
            [id = memberHybrids[i].first >> 8]( const Node::Ptr& n ) { return n->id == id; });

        if( source != nodes.end() )
        {
            auto sourceGen = source->get()->GetGenerator( nodes, dependancies, valid );
            if( metadata->memberHybrids[i].setNodeFunc( node.get(), sourceGen ) )
            {
                continue;
            }
            else
            {
                memberHybrids[i].first = -1;
            }
        }

        metadata->memberHybrids[i].setValueFunc( node.get(), memberHybrids[i].second );
    }

    for( int i = 0; i < metadata->memberVariables.size(); i++ )
    {
        metadata->memberVariables[i].setFunc( node.get(), memberValues[i] );
    }    


    return node;
}

FastNoiseNodeEditor::FastNoiseNodeEditor() :
    mNoiseImage( PixelFormat::RGBA8Srgb, { 0, 0 } )
{
    imnodes::Initialize();
}

void FastNoiseNodeEditor::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition )
{
    ImGui::SetNextWindowSize( ImVec2( 963, 634 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 8, 439 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "FastNoise Node Editor" ) )
    {
        UpdateSelected();

        bool edited = false;
        ImGui::SetNextItemWidth( 100 );
        edited |= ImGui::DragInt( "Seed", &mNodeSeed );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 100 );
        edited |= ImGui::DragFloat( "Frequency", &mNodeFrequency, 0.001f );

        if( edited )
        {
            for( Node::Ptr& node : mNodes )
            {
                node->GeneratePreview( this );
            }
            GenerateSelectedPreview();
        }

        imnodes::BeginNodeEditor();

        DoNodes();

        DoContextMenu();

        imnodes::EndNodeEditor();        

        // Check for new links
        int startAttr, endAttr;
        if( imnodes::IsLinkCreated( &startAttr, &endAttr ) )
        {
            for( Node::Ptr& n : mNodes )
            {
                int attrId = n->id << 8;

                for( int* id : n->memberLinks )
                {
                    if( attrId == startAttr )
                    {
                        *id = endAttr;
                        n->GeneratePreview( this );
                        break;
                    }
                    if( attrId == endAttr )
                    {
                        *id = startAttr;
                        n->GeneratePreview( this );
                        break;
                    }
                    attrId++;
                }
            }
        }    
    }
    ImGui::End();

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "FastNoise Preview" ) )
    {
        ImVec2 winSize = ImGui::GetContentRegionAvail();

        if(( mPreviewWindowsSize.x() != winSize.x || mPreviewWindowsSize.y() != winSize.y ) && winSize.x >= 1 && winSize.y >= 1 )
        {
            mPreviewWindowsSize.x() = (Int)winSize.x;
            mPreviewWindowsSize.y() = (Int)winSize.y;
            GenerateSelectedPreview();
        }

        ImGuiIntegration::image( mNoiseTexture, { (float)mNoiseImage.size().x(), (float)mNoiseImage.size().y() } );
    }
    ImGui::End();

    if( mSelectedNode != -1 )
    {
        mMeshNoisePreview.Draw( transformation, projection, cameraPosition );
    }
}

void FastNoiseNodeEditor::UpdateSelected()
{
    if( int selectedCount = imnodes::NumSelectedLinks() )
    {
        std::vector<int> selected( selectedCount );

        imnodes::GetSelectedLinks( selected.data() );

        if( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
        {
            for( int deleteID : selected )
            {
                for( auto& node : mNodes )
                {
                    bool changed = false;
                    int attributeId = node->id << 8;

                    for( int* link : node->memberLinks )
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
                        node->GeneratePreview( this );
                    }
                }
            }
        }
    }

    if( int selectedCount = imnodes::NumSelectedNodes() )
    {
        std::vector<int> selected( selectedCount );

        imnodes::GetSelectedNodes( selected.data() );

        if( selected[0] != mSelectedNode )
        {
            mSelectedNode = selected[0];
            GenerateSelectedPreview();
        }

        if( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
        {
            for( int deleteID : selected )
            {
                mNodes.erase( std::find_if( mNodes.begin(), mNodes.end(),
                                            [deleteID]( const Node::Ptr& n ) { return n->id == deleteID; } ) );

                for( auto& node : mNodes )
                {
                    bool changed = false;

                    for( int* link : node->memberLinks )
                    {
                        if( *link >> 8 == deleteID )
                        {
                            *link = -1;
                            changed = true;
                        }
                    }

                    if( changed )
                    {
                        node->GeneratePreview( this );
                    }
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

void FastNoiseNodeEditor::DoNodes()
{
    std::string className;

    for( auto& node : mNodes )
    {
        imnodes::BeginNode(node->id);

        imnodes::BeginNodeTitleBar();
        FormatClassName( className, node->metadata->name );
        ImGui::TextUnformatted( className.c_str() );
        imnodes::EndNodeTitleBar();        

        int attributeId = node->id << 8;

        for( auto& memberNode : node->metadata->memberNodes )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            ImGui::TextUnformatted( memberNode.name );
            imnodes::EndAttribute();
        }

        for( int i = 0; i < node->metadata->memberHybrids.size(); i++ )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            ImGui::PushItemWidth( 60.0f );

            bool isLinked = node->memberHybrids[i].first != -1;

            if( isLinked )
            {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if( ImGui::DragFloat( node->metadata->memberHybrids[i].name, &node->memberHybrids[i].second, 0.02f ) )
            {
                node->GeneratePreview( this );
            }

            if( isLinked )
            {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            imnodes::EndAttribute();
        }

        for( int i = 0; i < node->metadata->memberVariables.size(); i++ )
        {
            auto& nodeVar = node->metadata->memberVariables[i];
            ImGui::PushItemWidth( 60.0f );

            switch ( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
                {
                    if( ImGui::DragFloat( nodeVar.name, &node->memberValues[i].f, 0.02f, nodeVar.valueMin.f, nodeVar.valueMax.f ) )
                    {
                        node->GeneratePreview( this );
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EInt:
                {
                    if( ImGui::DragInt( nodeVar.name, &node->memberValues[i].i, 0.2f, nodeVar.valueMin.i, nodeVar.valueMax.i ) )
                    {
                        node->GeneratePreview( this );
                    }
                }
                break;
            case FastNoise::Metadata::MemberVariable::EEnum:
                {
                    if( ImGui::Combo( nodeVar.name, &node->memberValues[i].i, nodeVar.enumNames.data(), nodeVar.enumNames.size() ) )
                    {
                        node->GeneratePreview( this );
                    }
                }
                break;
            }
        }
            
        imnodes::BeginOutputAttribute( attributeId );
        ImGuiIntegration::image( node->noiseTexture, { (float)Node::NoiseSize, (float)Node::NoiseSize } );
        imnodes::EndAttribute();

        imnodes::EndNode();
    }

    // Do current node links
    for( Node::Ptr& node : mNodes )
    {
        int attributeId = node->id << 8;

        for( int* link : node->memberLinks )
        {
            if( *link != -1 )
            {
                imnodes::Link( attributeId, *link, attributeId );
            }
            attributeId++;
        }
    }
}

void FastNoiseNodeEditor::DoContextMenu()
{
    std::string className;
    ImVec2 drag = ImGui::GetMouseDragDelta( ImGuiMouseButton_Right );
    float distance = sqrtf( drag.x * drag.x + drag.y * drag.y );


    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2(4, 4) );
    if( distance < 5.0f && ImGui::BeginPopupContextWindow() )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        for( auto& metadata : FastNoise::MetadataManager::GetMetadataClasses() )
        {
            FormatClassName( className, metadata->name );
            if( ImGui::MenuItem( className.c_str() ) )
            {
                const int node_id = ++mCurrentNodeId;
                imnodes::SetNodeScreenSpacePos( node_id, startPos );
                
                mNodes.push_back( std::make_unique<Node>( metadata ) );
                
                Node::Ptr& node = mNodes.back();
                node->id = node_id;

                for( auto& value : metadata->memberVariables )
                {
                    node->memberValues.push_back( value.valueDefault );
                }

                node->memberNodes.reserve( metadata->memberNodes.size() );
                for( auto& value : metadata->memberNodes )
                {
                    node->memberNodes.emplace_back( -1 );
                    node->memberLinks.emplace_back( &node->memberNodes.back() );
                }

                node->memberHybrids.reserve( metadata->memberHybrids.size() );
                for( auto& value : metadata->memberHybrids )
                {
                    node->memberHybrids.emplace_back( -1, value.valueDefault );
                    node->memberLinks.emplace_back( &node->memberHybrids.back().first );
                }
                node->GeneratePreview( this );
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = std::find_if( mNodes.begin(), mNodes.end(),
        [id = mSelectedNode]( const Node::Ptr& n ) { return n->id == id; } );

    bool isValid = true;
    size_t noiseSize = mPreviewWindowsSize.x() * mPreviewWindowsSize.y();

    if( noiseSize )
    {
        delete[] mNoiseData;
        mNoiseData = new float[noiseSize];
    }

    if( find == mNodes.end() )
    {
        isValid = false;
    }
    else
    {
        auto gen = FastNoise::New<FastNoise::ConvertRGBA8>();
        std::vector<int> dependancies;

        auto nodeGen = find->get()->GetGenerator( mNodes, dependancies, isValid );

        if( isValid )
        {
            mMeshNoisePreview.ReGenerate( nodeGen, mNodeSeed );

            if( !noiseSize )
            {
                return;
            }

            float offset = mNodeFrequency * -0.5f;
            gen->SetSource( nodeGen );
            gen->GenUniformGrid2D( mNoiseData, mPreviewWindowsSize.y() * offset, mPreviewWindowsSize.x() * offset, mPreviewWindowsSize.y(), mPreviewWindowsSize.x(), mNodeFrequency, mNodeFrequency, mNodeSeed );

        }
    }

    if( !noiseSize )
    {
        return;
    }

    if( !isValid )
    {
        memset( mNoiseData, 0, sizeof( float ) * noiseSize );
    }

    Containers::ArrayView<float> view( mNoiseData, noiseSize );

    mNoiseImage = ImageView2D( PixelFormat::RGBA8Srgb, mPreviewWindowsSize, view );

    mNoiseTexture = GL::Texture2D();
    mNoiseTexture.setMagnificationFilter( GL::SamplerFilter::Linear )
        .setMinificationFilter( GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear )
        .setWrapping( GL::SamplerWrapping::ClampToEdge )
        .setMaxAnisotropy( GL::Sampler::maxMaxAnisotropy() )
        .setStorage( Math::log( mPreviewWindowsSize.x() )+ 1, GL::TextureFormat::RGBA8, mPreviewWindowsSize )
        .setSubImage( 0, {}, mNoiseImage )
        .generateMipmap();
}
