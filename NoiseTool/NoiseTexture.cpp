#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include "NoiseTexture.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include "ImGuiExtra.h"

using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration = 0;
    mBuildData.frequency = 0.02f;
    mBuildData.seed = 1337;
    mBuildData.size = { -1, -1 };
    mBuildData.offset = {};
    mBuildData.generationType = GenType_2D;

    for( size_t i = 0; i < 2; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }

    SetupSettingsHandlers();
}

NoiseTexture::~NoiseTexture()
{
    for( auto& thread : mThreads )
    {
        mGenerateQueue.KillThreads();
        thread.join();
    }
}

void NoiseTexture::Draw()
{
    TextureData texData;
    if( mCompleteQueue.Pop( texData ) )
    {
        if( mCurrentIteration < texData.iteration )
        {
            mCurrentIteration = texData.iteration;
            ImageView2D noiseImage( PixelFormat::RGBA8Srgb, texData.size, texData.textureData );
            SetPreviewTexture( noiseImage );
        }
        texData.Free();
    }

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        //ImGui::Text( "Min: %0.6f Max: %0.6f", mMinMax.min, mMinMax.max );

        ImGui::PushItemWidth( 82.0f );
        bool edited = ImGui::DragInt( "Seed", &mBuildData.seed );
        ImGui::SameLine();

        edited |= ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.001f );
        ImGui::SameLine();

        edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mBuildData.generationType ), GenTypeStrings );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.generationType ), GenType_Count );

        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        int texSize[2] = { mBuildData.size.x(), mBuildData.size.y() };
        ImGui::SameLine();

        if( ImGui::DragInt2( "Size", texSize, 2, 4, 8192 ) )
        {
            ImVec2 delta( (float)(texSize[0] - mBuildData.size.x()), (float)(texSize[1] - mBuildData.size.y()) );

            ImVec2 windowSize = ImGui::GetWindowSize();

            windowSize += delta;
            contentSize += delta;

            ImGui::SetWindowSize( windowSize );
        }

        ImGui::PopItemWidth();

        if( contentSize.x >= 1 && contentSize.y >= 1 &&
            (edited || mBuildData.size.x() != (int)contentSize.x || mBuildData.size.y() != (int)contentSize.y) )
        {
            Vector2i newSize = { (int)contentSize.x, (int)contentSize.y };

            mBuildData.offset.xy() -= Vector2( newSize - mBuildData.size ) / 2;
            mBuildData.size = newSize;
            ReGenerate( mBuildData.generator );
        }

        if( edited )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        ImGui::PushStyleColor( ImGuiCol_Button, 0 );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, 0 );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, 0 );
        ImGuiIntegration::imageButton( mNoiseTexture, Vector2( mNoiseTexture.imageSize( 0 ) ), {{},Vector2{1}}, 0 );
        ImGui::PopStyleColor( 3 );

        if( ImGui::IsItemHovered() )
        {
            Vector4 oldOffset = mBuildData.offset;

            if( mBuildData.generationType != GenType_2DTiled && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Left ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Left );

                mBuildData.offset.x() -= dragDelta.x();
                mBuildData.offset.y() += dragDelta.y();
            }
            else if( (mBuildData.generationType == GenType_3D || mBuildData.generationType == GenType_4D)
                && ImGui::IsMouseDragging( ImGuiMouseButton_Right ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Right );

                mBuildData.offset.z() -= dragDelta.x();

                if( mBuildData.generationType == GenType_4D )
                {
                    mBuildData.offset.w() -= dragDelta.y();
                }
            }

            if( oldOffset != mBuildData.offset )
            {
                ReGenerate( mBuildData.generator );
            }
        }
    }
    ImGui::End();
}

void NoiseTexture::SetPreviewTexture( ImageView2D& imageView )
{
    mNoiseTexture = GL::Texture2D();
    mNoiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, imageView.size() )
        .setSubImage( 0, {}, imageView );
}

void NoiseTexture::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mBuildData.generator = generator;
    mBuildData.iteration++;

    mGenerateQueue.Clear();

    if( mBuildData.size.x() <= 0 || mBuildData.size.y() <= 0 )
    {
        return;
    }

    if( generator )
    {
        mGenerateQueue.Push( mBuildData );
        return;
    }

    std::array<uint32_t, 16 * 16> blankTex = {};

    ImageView2D noiseImage( PixelFormat::RGBA8Unorm, {16,16}, blankTex );
    mCurrentIteration = mBuildData.iteration;

    SetPreviewTexture( noiseImage );
}


NoiseTexture::TextureData NoiseTexture::BuildTexture( const BuildData& buildData )
{
    static thread_local std::vector<float> noiseData;
    noiseData.resize( (size_t)buildData.size.x() * buildData.size.y() );

    auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( buildData.generator->GetSIMDLevel() );

    genRGB->SetSource( buildData.generator );
    FastNoise::OutputMinMax minMax;

    switch( buildData.generationType )
    {
    case GenType_2D:
        minMax = genRGB->GenUniformGrid2D( noiseData.data(), 
            (int)buildData.offset.x(), (int)buildData.offset.y(),
            buildData.size.x(), buildData.size.y(),
            buildData.frequency, buildData.seed );
        break;

    case GenType_2DTiled:
        minMax = genRGB->GenTileable2D( noiseData.data(),
            buildData.size.x(), buildData.size.y(),
            buildData.frequency, buildData.seed );
        break;

    case GenType_3D:
        minMax = genRGB->GenUniformGrid3D( noiseData.data(),
            (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(),
            buildData.size.x(), buildData.size.y(), 1,
            buildData.frequency, buildData.seed );
        break;

    case GenType_4D:
        minMax = genRGB->GenUniformGrid4D( noiseData.data(),
            (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(), (int)buildData.offset.w(),
            buildData.size.x(), buildData.size.y(), 1, 1,
            buildData.frequency, buildData.seed );
        break;
    }

    return TextureData( buildData.iteration, buildData.size, minMax, noiseData );
}

void NoiseTexture::GenerateLoopThread( GenerateQueue<BuildData>& generateQueue, CompleteQueue<TextureData>& completeQueue )
{
    while( true )
    {
        BuildData buildData = generateQueue.Pop();

        if( generateQueue.ShouldKillThread() )
        {
            return;
        }

        TextureData texData = BuildTexture( buildData );

        if( !completeQueue.Push( texData ) )
        {
            texData.Free();
        }
    }
}

void NoiseTexture::SetupSettingsHandlers()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName = "NoiseToolNoiseTexture";
    editorSettings.TypeHash = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );        

        outBuf->appendf( "frequency=%f\n", noiseTexture->mBuildData.frequency );
        outBuf->appendf( "seed=%d\n", noiseTexture->mBuildData.seed );
        outBuf->appendf( "gen_type=%d\n", (int)noiseTexture->mBuildData.generationType );
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;
        
        sscanf_s( line, "frequency=%f", &noiseTexture->mBuildData.frequency );
        sscanf_s( line, "seed=%d", &noiseTexture->mBuildData.seed );
        sscanf_s( line, "gen_type=%d", &noiseTexture->mBuildData.generationType );
    };

    ImGui::GetCurrentContext()->SettingsHandlers.push_back( editorSettings );
}