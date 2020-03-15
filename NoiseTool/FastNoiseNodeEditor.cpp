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

void FastNoiseNodeEditor::Node::GeneratePreview( std::vector<Node>& nodes )
{
    auto gen = FastNoise::New<FastNoise::ConvertRGBA8>();

    bool isValid = true;
    gen->SetSource( GetGenerator( nodes, isValid ) );

    if( isValid )
    {
        gen->GenUniformGrid2D( noiseData, 0, 0, NoiseSize, NoiseSize, 0.05f, 0.05f, 1337 );
    }
    else
    {
        memset( noiseData, 0, sizeof( noiseData ));
    }

    noiseImage.setData( noiseData );
    
    noiseTexture.setMagnificationFilter(GL::SamplerFilter::Linear)
        .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
        .setStorage(Math::log2( NoiseSize ) + 1, GL::TextureFormat::RGBA8, { NoiseSize, NoiseSize })
        .setSubImage(0, {}, noiseImage)
        .generateMipmap();
}

std::shared_ptr<FastNoise::Generator> FastNoiseNodeEditor::Node::GetGenerator( std::vector<Node>& nodes, bool& valid )
{
    std::shared_ptr<FastNoise::Generator> node( metadata->NodeFactory() );

    for( int i = 0; i < metadata->memberNodes.size(); i++ )
    {
        auto source = std::find_if( nodes.begin(), nodes.end(),
            [id = memberNodes[i] >> 8] ( const Node& n ) { return n.id == id; } );

        if (source == nodes.end())
        {
            memberNodes[i] = -1;
            valid = false;
        }
        else
        {
            metadata->memberNodes[i].setFunc( node.get(), source->GetGenerator( nodes, valid ) );
        }
    }

    return node;
}

void FastNoiseNodeEditor::Update()
{
    ImGui::SetNextWindowSize( ImVec2(800, 600), ImGuiCond_Always );
    ImGui::Begin( "FastNoise Node Editor" );

    imnodes::BeginNodeEditor();

    for( Node& node : mNodes )
    {
        imnodes::BeginNode(node.id);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted( node.metadata->name );
        imnodes::EndNodeTitleBar();
        

        int attributeId = node.id << 8;

        for( auto& memberNode : node.metadata->memberNodes )
        {
            imnodes::BeginInputAttribute( attributeId++ );
            ImGui::TextUnformatted( memberNode.name );
            imnodes::EndAttribute();
        }

        for( int i = 0; i < node.metadata->memberVariables.size(); i++ )
        {
            auto& nodeVar = node.metadata->memberVariables[i];

            switch ( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
            {
                ImGui::PushItemWidth( 60.0f );
                if( ImGui::DragFloat( nodeVar.name, &node.memberValues[i].f ) )
                {

                }
            }
            break;
            case FastNoise::Metadata::MemberVariable::EInt:
            {
                ImGui::PushItemWidth( 60.0f );
                if( ImGui::DragInt( nodeVar.name, &node.memberValues[i].i ) )
                {

                }
            }
            break;
            }
        }

        /*ImGui::PushItemWidth(120.0f);
        ImGui::DragFloat("value", &node.value, 0.01f);
        ImGui::PopItemWidth();*/

        imnodes::BeginOutputAttribute( attributeId );
        /*const float text_width = ImGui::CalcTextSize( "Output" ).x;
        ImGui::Indent(ImGui::GetWindowWidth() - text_width);
        ImGui::TextUnformatted( "Output" );

        ImGui::Indent();*/
        ImGuiIntegration::image( node.noiseTexture, { Node::NoiseSize, Node::NoiseSize } );
        imnodes::EndAttribute();

        imnodes::EndNode();
    }
    
    for( Node& node : mNodes )
    {
        int attributeId = node.id << 8;

        for( int i = 0; i < node.metadata->memberNodes.size(); i++  )
        {
            if( node.memberNodes[i] != -1 )
            {                
                imnodes::Link( attributeId, node.memberNodes[i], attributeId++ );
            }
        }
    }

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2(4, 4) );
    if( ImGui::BeginPopupContextWindow() )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        for( auto& metadata : FastNoise::MetadataManager::GetMetadataClasses() )
        {
            if (ImGui::MenuItem(metadata->name))
            {
                const int node_id = ++mCurrentNodeId;
                imnodes::SetNodeScreenSpacePos(node_id, startPos);
                
                mNodes.push_back( Node( metadata ) );
                
                Node& node = mNodes.back();
                node.id = node_id;

                node.memberNodes = std::vector<int>( metadata->memberNodes.size(), -1 );

                for( auto& value : metadata->memberVariables )
                {
                    node.memberValues.push_back( value.valueDefault );
                }
                node.GeneratePreview( mNodes );
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    imnodes::EndNodeEditor();
        
    int startAttr, endAttr;
    if( imnodes::IsLinkCreated( &startAttr, &endAttr ) )
    {
        for( Node& n : mNodes )
        {
            int attrId = n.id << 8;

            for( int& id : n.memberNodes )
            {
                if( attrId == startAttr )
                {
                    id = endAttr;
                    n.GeneratePreview( mNodes );
                    break;
                }
                if( attrId++ == endAttr )
                {
                    id = startAttr;
                    n.GeneratePreview( mNodes );
                    break;
                }
            }
        }
    }    

    ImGui::End();
}
