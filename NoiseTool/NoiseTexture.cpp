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
    mBuildData.offset = { 0, 0, 0 };
    mBuildData.generationType = BuildData::GenType_2D;

    for( size_t i = 0; i < 2; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }
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

        edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mBuildData.generationType ), "2D\0" "2D Tiled\0" "3D Slice\0" );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.generationType ), 3 );

        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        int texSize[2] = { mBuildData.size.x(), mBuildData.size.y() };
        ImGui::SameLine();

        if( ImGui::DragInt2( "Size", texSize, 2, 16, 8192 ) )
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
            mBuildData.size = { (int)contentSize.x, (int)contentSize.y };
            ReGenerate( mBuildData.generator );
        }

        ImGuiIntegration::image( mNoiseTexture, Vector2( mNoiseTexture.imageSize( 0 ) ) );
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
    noiseData.resize( buildData.size.x() * buildData.size.y() );

    auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( buildData.generator->GetSIMDLevel() );

    genRGB->SetSource( buildData.generator );
    FastNoise::OutputMinMax minMax;

    switch( buildData.generationType )
    {
    case BuildData::GenType_2D:
        minMax = genRGB->GenUniformGrid2D( noiseData.data(),
            buildData.size.x() / -2, buildData.size.y() / -2,
            buildData.size.x(), buildData.size.y(),
            buildData.frequency, buildData.seed );
        break;

    case BuildData::GenType_2DTiled:
        minMax = genRGB->GenTileable2D( noiseData.data(),
            buildData.size.x(), buildData.size.y(),
            buildData.frequency, buildData.seed );
        break;

    case BuildData::GenType_3D:
        minMax = genRGB->GenUniformGrid3D( noiseData.data(),
            buildData.size.x() / -2, buildData.size.y() / -2, 0,
            buildData.size.x(), buildData.size.y(), 1,
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
