#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <sstream>
#else
#include <filesystem>
#include <fstream>
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <FastNoise/Metadata.h>

#include "util/ImGuiExtra.h"
#include "NoiseTexture.h"


using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration = 0;
    mBuildData.scale = 1.f;
    mBuildData.seed = 1337;
    mBuildData.size = { -1, -1 };
    mBuildData.offset = {};
    mBuildData.generationType = GenType_2D;

    mExportBuildData.size = { 4096, 4096 };

    for( size_t i = 0; i < 2; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }

    Debug{} << "Texture generator thread count: " << mThreads.size();

    SetupSettingsHandlers();
}

NoiseTexture::~NoiseTexture()
{
    for( auto& thread : mThreads )
    {
        mGenerateQueue.KillThreads();
        thread.join();
    }
    
    if( mExportThread.joinable() )
    {
        mExportThread.join();
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
            ImageView2D noiseImage( PixelFormat::RGBA8Unorm, texData.size, texData.textureData );
            SetPreviewTexture( noiseImage );
        }
        texData.Free();
    }

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Output", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar ) )
    {
        //ImGui::Text( "Min: %0.6f Max: %0.6f", mMinMax.min, mMinMax.max );
        
        bool edited = false;
        ImVec2 windowSizeDelta = { 0, 0 };
        
        // Menu bar dropdown
        if( ImGui::BeginMenuBar() )
        {
            if( ImGui::BeginMenu( "Settings" ) )
            {
                ImGui::PushItemWidth( 120.0f );
                
                edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mBuildData.generationType ), GenTypeStrings );
                edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.generationType ), GenType_Count );
                
                edited |= ImGui::DragInt( "Seed", &mBuildData.seed );

                Vector2i texSize = { mBuildData.size.x(), mBuildData.size.y() };
                if( ImGui::DragInt2( "Size", texSize.data(), 2, 4, 8192 ) )
                {
                    ImVec2 delta( Vector2{ texSize - mBuildData.size } );
                    windowSizeDelta = delta;
                }

                // Scale control - offset is already the center point in noise space,
                // so no adjustment is needed to maintain visual center when scale changes
                edited |= ImGui::DragFloat( "Scale", &mBuildData.scale, 0.01f );

                // Offset controls
                if( mBuildData.generationType != GenType_2DTiled )
                {
                    Vector2 xyOffset = { mBuildData.offset.x(), mBuildData.offset.y() };
                    if( ImGui::DragFloat2( "Center X Y", xyOffset.data(), 1.0f ) )
                    {
                        mBuildData.offset.x() = xyOffset.x();
                        mBuildData.offset.y() = xyOffset.y();
                        edited = true;
                    }
                    if( ImGui::IsItemHovered() )
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted( "Left mouse drag on preview to pan X Y" );
                        ImGui::EndTooltip();
                    }
                }
                
                // Show Z, W offset for 3D and 4D generation types
                if( mBuildData.generationType == GenType_3D )
                {
                    // 3D mode: just Z offset
                    float zOffset = mBuildData.offset.z();
                    if( ImGui::DragFloat( "Center Z", &zOffset, 1.0f ) )
                    {
                        mBuildData.offset.z() = zOffset;
                        edited = true;
                    }
                    if( ImGui::IsItemHovered() )
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted( "Right mouse drag left/right on preview to change Z" );
                        ImGui::EndTooltip();
                    }
                }
                else if( mBuildData.generationType == GenType_4D )
                {
                    // 4D mode: Z and W together
                    Vector2 zwOffset = { mBuildData.offset.z(), mBuildData.offset.w() };
                    if( ImGui::DragFloat2( "Center Z W", zwOffset.data(), 1.0f ) )
                    {
                        mBuildData.offset.z() = zwOffset.x();
                        mBuildData.offset.w() = zwOffset.y();
                        edited = true;
                    }
                    if( ImGui::IsItemHovered() )
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted( "Right mouse drag on preview to change Z and W" );
                        ImGui::EndTooltip();
                    }
                }
                
                
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }
            
            if( ImGui::BeginMenu( mIsExporting ? "Exporting..." : "Export" ) )
            {
                DrawExportMenu();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        // Get content size AFTER menu bar has been processed (or would have been processed)
        ImVec2 contentSize = ImGui::GetContentRegionAvail() + windowSizeDelta;
        
        if( edited )
        {
            ImGuiExtra::MarkSettingsDirty();
        }
        
        if( contentSize.x >= 1 && contentSize.y >= 1 &&
            (edited || mBuildData.size.x() != (int)contentSize.x || mBuildData.size.y() != (int)contentSize.y) )
        {
            Vector2i newSize = { (int)contentSize.x, (int)contentSize.y };

            mBuildData.size = newSize;
            ReGenerate( mBuildData.generator );
            
            ImGui::SetWindowSize( ImGui::GetWindowSize() + windowSizeDelta );
        }

        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImGuiIntegration::image( mNoiseTexture, Vector2( mBuildData.size ) );
        
        // Add invisible button over the image to capture mouse input (since image widget doesn't capture input)
        ImGui::SetCursorPos( cursorPos );
        ImGui::InvisibleButton( "noise_texture_input", ImVec2( (float)mBuildData.size.x(), (float)mBuildData.size.y() ) );

        if( ImGui::IsItemHovered() )
        {
            Vector4 oldOffset = mBuildData.offset;

            if( mBuildData.generationType != GenType_2DTiled && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Left ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Left );

                // Scale the drag delta to match the current scale factor
                mBuildData.offset.x() -= dragDelta.x() * mBuildData.scale;
                mBuildData.offset.y() += dragDelta.y() * mBuildData.scale;
            }
            else if( (mBuildData.generationType == GenType_3D || mBuildData.generationType == GenType_4D)
                && ImGui::IsMouseDragging( ImGuiMouseButton_Right ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Right );

                // Scale the drag delta to match the current scale factor
                mBuildData.offset.z() -= dragDelta.x() * mBuildData.scale;

                if( mBuildData.generationType == GenType_4D )
                {
                    mBuildData.offset.w() += dragDelta.y() * mBuildData.scale;
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

void NoiseTexture::DrawExportMenu()
{
    ImGui::PushItemWidth( 120.0f );
    
    if( ImGui::DragInt2( "Export Size", mExportBuildData.size.data(), 2, 4, 8192 * 4 ) )
    {
        ImGuiExtra::MarkSettingsDirty();
    }
    
    // Filename input field
    char filenameBuffer[256];
    std::string displayFilename = mExportFilename;
    
    // If no custom filename is set, show the node name as default
    if( displayFilename.empty() && mBuildData.generator )
    {
        displayFilename = mBuildData.generator->GetMetadata().name;
    }
    
    strncpy( filenameBuffer, displayFilename.c_str(), sizeof(filenameBuffer) - 1 );
    filenameBuffer[sizeof(filenameBuffer) - 1] = '\0';
    
    if( ImGui::InputText( "Filename", filenameBuffer, sizeof(filenameBuffer) ) )
    {
        std::string newFilename = std::string( filenameBuffer );
        
        // Only save as custom filename if it's different from the node name
        if( mBuildData.generator && newFilename != mBuildData.generator->GetMetadata().name )
        {
            mExportFilename = newFilename;
        }
        else if( !mBuildData.generator )
        {
            mExportFilename = newFilename;
        }
        else
        {
            mExportFilename = "";  // Clear custom filename if user set it back to node name
        }
        
        ImGuiExtra::MarkSettingsDirty();
    }
    
    if( ImGui::Checkbox( "Scale export to match preview area", &mUseRelativeScaling ) )
    {
        ImGuiExtra::MarkSettingsDirty();
    }
    
    ImGui::Separator();
    
    ImGui::BeginDisabled( mIsExporting || !mBuildData.generator );
    if( ImGui::MenuItem( "Export as BMP" ) )
    {
        mExportBuildData.generationType = mBuildData.generationType;
        mExportBuildData.seed = mBuildData.seed;
        mExportBuildData.generator = mBuildData.generator;
        
        if( mUseRelativeScaling )
        {
            float relativeScale = (float)mExportBuildData.size.sum() / mBuildData.size.sum();
            mExportBuildData.scale = mBuildData.scale / relativeScale;
            mExportBuildData.offset = mBuildData.offset * relativeScale;
        }
        else
        {
            mExportBuildData.scale = mBuildData.scale;
            mExportBuildData.offset = mBuildData.offset;
        }

        if( mExportThread.joinable() )
        {
            mExportThread.join();
        }
        mIsExporting.store( true, std::memory_order::relaxed );

        mExportThread = std::thread([buildData = mExportBuildData, customFilename = mExportFilename, this]()
        {
            Debug{} << "BMP Export Started";
            auto data = BuildTexture( buildData );

            std::string filename;
            if( !customFilename.empty() )
            {
                filename = customFilename;
                // Add .bmp extension if not already present
                if( filename.length() < 4 || filename.substr( filename.length() - 4 ) != ".bmp" )
                {
                    filename += ".bmp";
                }
            }
            else
            {
                const char* nodeName = buildData.generator->GetMetadata().name;
                filename = nodeName;
                filename += ".bmp";
            }

#ifdef __EMSCRIPTEN__
            std::stringstream file;
#else
            // Get absolute path for console output
            std::filesystem::path fullPath = std::filesystem::absolute( filename );
            
            // Iterate through file names if filename exists
            for( int i = 1; i < 1024; i++ )
            {
                if( !std::filesystem::exists( filename.c_str() ) )
                {
                    fullPath = std::filesystem::absolute( filename );
                    break;
                }
                
                // Create numbered version
                size_t dotPos = filename.find_last_of( '.' );
                if( dotPos != std::string::npos )
                {
                    std::string baseName = filename.substr( 0, dotPos );
                    std::string extension = filename.substr( dotPos );
                    filename = baseName + '_' + std::to_string( i ) + extension;
                }
                else
                {
                    filename += '_' + std::to_string( i );
                }
            }   
            
            fullPath = std::filesystem::absolute( filename );

            std::ofstream file( filename.c_str(), std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );

            if( file.is_open() )
#endif
            {
                struct BmpHeader
                {
                    // File header (14)
                    // char b = 'B';
                    // char m = 'M';
                    uint32_t fileSize;
                    uint32_t reserved = 0;
                    uint32_t dataOffset = 14u + 12u + (256u * 3u);
                    // Bmp Info Header (12)
                    uint32_t headerSize = 12u;
                    uint16_t sizeX;
                    uint16_t sizeY;
                    uint16_t colorPlanes = 1u;
                    uint16_t bitDepth = 8u;
                };

                int paddedSizeX = buildData.size.x();
                int padding = paddedSizeX % 4;
                if( padding )
                {
                    padding = 4 - padding;
                    paddedSizeX += padding;
                }

                BmpHeader header;
                header.fileSize = header.dataOffset + (uint32_t)(paddedSizeX * buildData.size.y());
                header.sizeX = (uint16_t)buildData.size.x();
                header.sizeY = (uint16_t)buildData.size.y();

                file << 'B' << 'M';
                file.write( reinterpret_cast<char*>( &header ), sizeof( BmpHeader ) );

                // Colour map
                for (int i = 0; i < 256; i++)
                {
                    Vector3ub b3( (uint8_t)i );
                    file.write( reinterpret_cast<char*>( b3.data() ), 3 );
                }

                int xIdx = padding ? buildData.size.x() : 0;

                for( uint32_t pix : data.textureData ) 
                {
                    file.write( reinterpret_cast<char*>( &pix ), 1 );

                    if( --xIdx == 0 )
                    {
                        xIdx = buildData.size.x();

                        Vector3ub b3( 0 );
                        file.write( reinterpret_cast<char*>( b3.data() ), padding );                        
                    }
                }

#ifdef __EMSCRIPTEN__
                std::string_view fileString = file.view();

                MAIN_THREAD_EM_ASM( (
                    // Create a temporary ArrayBuffer and copy the contents of the shared buffer
                    // into it.
                    const tempBuffer = new ArrayBuffer( $2 );
                    const tempView = new Uint8Array( tempBuffer );

                    let sharedView = new Uint8Array( wasmMemory.buffer, $1, $2 );
                    tempView.set( sharedView );

                    /// Offer a buffer in memory as a file to download, specifying download filename and mime type
                    var a = document.createElement( 'a' );
                    a.download = UTF8ToString( $0 );
                    a.href = URL.createObjectURL( new Blob( [tempView], {type: 'image/bmp'} ) );
                    a.style.display = 'none';
                    document.body.appendChild( a );
                    a.click();
                    document.body.removeChild( a );
                    URL.revokeObjectURL( a.href );
                    ), filename.c_str(), fileString.data(), fileString.length() );
                    
                std::string_view fileSaveString = filename;
#else
                file.close();
                std::string fileSaveString = fullPath.string();
#endif

                Debug{} << "BMP Export Complete:" << fileSaveString.data();
                mIsExporting = false;
            }
        } );
    }
    ImGui::EndDisabled();
    
    ImGui::PopItemWidth();
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

    auto gen = FastNoise::New<FastNoise::ConvertRGBA8>( buildData.generator->GetActiveFeatureSet() );
    gen->SetSource( buildData.generator );

    FastNoise::OutputMinMax minMax;

    float xOffset = buildData.offset.x() - (buildData.size.x() / 2.0f) * buildData.scale;
    float yOffset = buildData.offset.y() - (buildData.size.y() / 2.0f) * buildData.scale;

    switch( buildData.generationType )
    {
    case GenType_2D:
        minMax = gen->GenUniformGrid2D( noiseData.data(), 
            xOffset, yOffset,
            buildData.size.x(), buildData.size.y(), buildData.scale, buildData.scale, buildData.seed );
        break;

    case GenType_2DTiled:
        minMax = gen->GenTileable2D( noiseData.data(),
            buildData.size.x(), buildData.size.y(), buildData.scale, buildData.scale, buildData.seed );
        break;

    case GenType_3D:
        minMax = gen->GenUniformGrid3D( noiseData.data(),
            xOffset, yOffset, buildData.offset.z(),
            buildData.size.x(), buildData.size.y(), 1, buildData.scale, buildData.scale, buildData.scale, buildData.seed );
        break;

    case GenType_4D:
        minMax = gen->GenUniformGrid4D( noiseData.data(),
            xOffset, yOffset, buildData.offset.z(), buildData.offset.w(),
            buildData.size.x(), buildData.size.y(), 1, 1, buildData.scale, buildData.scale, buildData.scale, buildData.scale, buildData.seed );
        break;
    case GenType_Count:
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
    editorSettings.TypeName = "NodeEditorNoiseTexture";
    editorSettings.TypeHash = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );        

        outBuf->appendf( "scale=%f\n", noiseTexture->mBuildData.scale );
        outBuf->appendf( "seed=%d\n", noiseTexture->mBuildData.seed );
        outBuf->appendf( "gen_type=%d\n", (int)noiseTexture->mBuildData.generationType );
        outBuf->appendf( "export_size=%d:%d\n", noiseTexture->mExportBuildData.size.x(), noiseTexture->mExportBuildData.size.y() );
        outBuf->appendf( "use_relative_scaling=%d\n", noiseTexture->mUseRelativeScaling ? 1 : 0 );
        outBuf->appendf( "export_filename=%s\n", noiseTexture->mExportFilename.c_str() );
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
        
        sscanf( line, "scale=%f", &noiseTexture->mBuildData.scale );
        sscanf( line, "seed=%d", &noiseTexture->mBuildData.seed );
        sscanf( line, "gen_type=%d", (int*)&noiseTexture->mBuildData.generationType );
        sscanf( line, "export_size=%d:%d", &noiseTexture->mExportBuildData.size.x() , &noiseTexture->mExportBuildData.size.y() );
        
        int useRelativeScaling = 0;
        if( sscanf( line, "use_relative_scaling=%d", &useRelativeScaling ) == 1 )
        {
            noiseTexture->mUseRelativeScaling = useRelativeScaling != 0;
        }
        
        // Read export filename (skip "export_filename=" prefix)
        if( strncmp( line, "export_filename=", 16 ) == 0 )
        {
            noiseTexture->mExportFilename = std::string( line + 16 );
        }
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}
