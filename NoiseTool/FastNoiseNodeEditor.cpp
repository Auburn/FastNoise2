#include "FastNoiseNodeEditor.h"
#include "imnodes.h"

#include <imgui.h>
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
        gen->GenUniformGrid2D( noiseData, 0, 0, NoiseSize, NoiseSize, editor->mNodeFrequency, editor->mNodeFrequency, editor->mNodeSeed );
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
        for( int link : node->memberNodes )
        {
            if( link >> 8 == id )
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
    
    for( int i = 0; i < metadata->memberNodes.size(); i++ )
    {
        if( std::find( dependancies.begin(), dependancies.end(), memberNodes[i] >> 8 ) != dependancies.end() )
        {
            memberNodes[i] = -1;
            valid = false;
        }
        else
        {
            auto source = std::find_if(nodes.begin(), nodes.end(),
                [id = memberNodes[i] >> 8](const Node::Ptr& n) { return n->id == id; });

            if( source == nodes.end() || !metadata->memberNodes[i].setFunc( node.get(), source->get()->GetGenerator( nodes, dependancies, valid ) ) )
            {
                valid = false;
            }
        }
    }

    for (int i = 0; i < metadata->memberVariables.size(); i++)
    {
        metadata->memberVariables[i].setFunc(node.get(), memberValues[i]);
    }    


    return node;
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

Magnum::FastNoiseNodeEditor::FastNoiseNodeEditor() :
    mNoiseImage( PixelFormat::RGBA8Srgb, { 0, 0 } )
{

}

void FastNoiseNodeEditor::Update()
{
    ImGui::SetNextWindowSize( ImVec2( 800, 600 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "FastNoise Node Editor" ) )
    {
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

            for( int i = 0; i < node->metadata->memberVariables.size(); i++ )
            {
                auto& nodeVar = node->metadata->memberVariables[i];

                switch ( nodeVar.type )
                {
                case FastNoise::Metadata::MemberVariable::EFloat:
                {
                    ImGui::PushItemWidth( 60.0f );
                    if( ImGui::DragFloat( nodeVar.name, &node->memberValues[i].f, 0.02f, node->metadata->memberVariables[i].valueMin.f, node->metadata->memberVariables[i].valueMax.f ) )
                    {
                        node->GeneratePreview( this );
                    }
                }
                break;
                case FastNoise::Metadata::MemberVariable::EInt:
                {
                    ImGui::PushItemWidth( 60.0f );
                    if( ImGui::DragInt( nodeVar.name, &node->memberValues[i].i, 0.2f, node->metadata->memberVariables[i].valueMin.i, node->metadata->memberVariables[i].valueMax.i ) )
                    {
                        node->GeneratePreview( this );
                    }
                }
                break;
                }
            }
            
            imnodes::BeginOutputAttribute( attributeId );
            ImGuiIntegration::image( node->noiseTexture, { Node::NoiseSize, Node::NoiseSize } );
            imnodes::EndAttribute();

            imnodes::EndNode();
        }
    
        for( Node::Ptr& node : mNodes )
        {
            int attributeId = node->id << 8;

            for( int i = 0; i < node->metadata->memberNodes.size(); i++  )
            {
                if( node->memberNodes[i] != -1 )
                {                
                    imnodes::Link( attributeId, node->memberNodes[i], attributeId );
                }
                attributeId++;
            }
        }

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

                    node->memberNodes = std::vector<int>( metadata->memberNodes.size(), -1 );

                    for( auto& value : metadata->memberVariables )
                    {
                        node->memberValues.push_back( value.valueDefault );
                    }
                    node->GeneratePreview( this );
                }
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        imnodes::EndNodeEditor();

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
                        for( int& link : node->memberNodes )
                        {
                            if( link >> 8 == deleteID )
                            {
                                link = -1;
                                node->GeneratePreview( this );
                            }
                        }
                    }
                }
            }
        }

        int startAttr, endAttr;
        if( imnodes::IsLinkCreated( &startAttr, &endAttr ) )
        {
            for( Node::Ptr& n : mNodes )
            {
                int attrId = n->id << 8;

                for( int& id : n->memberNodes )
                {
                    if( attrId == startAttr )
                    {
                        id = endAttr;
                        n->GeneratePreview( this );
                        break;
                    }
                    if( attrId == endAttr )
                    {
                        id = startAttr;
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
}

void FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = std::find_if( mNodes.begin(), mNodes.end(),
        [id = mSelectedNode]( const Node::Ptr& n ) { return n->id == id; } );

    bool isValid = true;
    size_t noiseSize = mPreviewWindowsSize.x() * mPreviewWindowsSize.y();

    if( !noiseSize )
    {
        return;
    }

    if( mNoiseData )
    {
        delete[] mNoiseData;
    }
    mNoiseData = new float[noiseSize];

    if( find == mNodes.end() )
    {
        isValid = false;
    }
    else
    {
        auto gen = FastNoise::New<FastNoise::ConvertRGBA8>();

        gen->SetSource( find->get()->GetGenerator( mNodes, std::vector<int>(), isValid ) );

        if( isValid )
        {
            gen->GenUniformGrid2D( mNoiseData, 0, 0, mPreviewWindowsSize.y(), mPreviewWindowsSize.x(), mNodeFrequency, mNodeFrequency, mNodeSeed );
        }
    }

    if( !isValid )
    {
        memset( mNoiseData, 0, sizeof( float ) * noiseSize );
    }

    Corrade::Containers::ArrayView<float> view( mNoiseData, noiseSize );

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
