#include <imgui.h>

#include "NoiseTexture.h"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration = 0;
    mBuildData.frequency = 0.02f;
    mBuildData.seed = 1337;
    mBuildData.genVersion = 0;
    mBuildData.size = { -1, -1 };
    mBuildData.offset = { 0, 0, 0 };

    for( size_t i = 0; i < 2; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }
}

NoiseTexture::~NoiseTexture()
{
    for( auto& thread : mThreads )
    {
        thread.detach();
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
            ImageView2D noiseImage( PixelFormat::RGBA8Srgb, mBuildData.size, texData.textureData );
            SetPreviewTexture( noiseImage );
        }
        texData.Free();
    }

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Preview" ) )
    {
        std::string serialised;
        //ImGui::Text( "Min: %0.6f Max: %0.6f", mMinMax.min, mMinMax.max );

        ImGui::SetNextItemWidth( 100 );
        bool edited = ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.001f );
        ImGui::SameLine();

        const char* serialisedLabel = "Encoded Node Tree";
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize( serialisedLabel ).x );
        ImGui::InputText( serialisedLabel, serialised.data(), serialised.size(), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

        ImVec2 winSize = ImGui::GetContentRegionAvail();

        if( winSize.x >= 1 && winSize.y >= 1 &&
            (edited || mBuildData.size.x() != winSize.x || mBuildData.size.y() != winSize.y) )
        {
            mBuildData.size.x() = (int)winSize.x;
            mBuildData.size.y() = (int)winSize.y;
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

void NoiseTexture::ReGenerate( const std::shared_ptr<FastNoise::Generator>& generator )
{
    mBuildData.generator = generator;
    mBuildData.iteration++;

    mGenerateQueue.Clear();

    uint32_t newVersion = mBuildData.size.x() ^ (mBuildData.size.y() << 16);
    if( newVersion != mBuildData.genVersion )
    {
        mBuildData.genVersion = newVersion;
        mCompleteQueue.SetVersion( newVersion );

        TextureData texData;
        while( mCompleteQueue.Pop( texData ) )
        {
            texData.Free();
        }
    }

    size_t noiseSize = (size_t)mBuildData.size.x() * mBuildData.size.y();
    if( !noiseSize )
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

    auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>();

    genRGB->SetSource( buildData.generator );
    FastNoise::OutputMinMax minMax = genRGB->GenUniformGrid2D( noiseData.data(),
        buildData.size.x() / -2, buildData.size.y() / -2,
        buildData.size.x(), buildData.size.y(),
        buildData.frequency, buildData.seed );

    return TextureData( buildData.iteration, minMax, noiseData );
}

void NoiseTexture::GenerateLoopThread( GenerateQueue<BuildData>& generateQueue, CompleteQueue<TextureData>& completeQueue )
{
    while( true )
    {
        BuildData buildData = generateQueue.Pop();

        TextureData texData = BuildTexture( buildData );

        if( !completeQueue.Push( texData, buildData.genVersion ) )
        {
            texData.Free();
        }
    }
}
