#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "FastNoise/NodeEditorIpc_C.h"
#include "FastSIMD/FastSIMD_FastNoise_config.h"
#include "NodeEditorApp.h"
#include "util/ImGuiExtra.h"

using namespace Magnum;

static constexpr const char* kAppSettingsFile = FILESYSTEM_ROOT "NodeEditor.ini";

void InitResources()
{
#ifdef MAGNUM_BUILD_STATIC
    CORRADE_RESOURCE_INITIALIZE( NodeEditor_RESOURCES )
#endif
}

static bool IsDetached( const NodeEditorApp::Arguments& arguments )
{
    return arguments.argc > 1 && std::string_view { arguments.argv[1] } == "-detached";
}

NodeEditorApp::NodeEditorApp( const Arguments& arguments ) :
    Platform::Application{ arguments,
        Configuration{}
        .setTitle( IsDetached( arguments ) ? "FastNoise2 Node Graph" : "FastNoise2 Node Editor" )
#ifdef __EMSCRIPTEN__
        .setWindowFlags( Configuration::WindowFlag::Resizable )
#else
        .setSize( Vector2i( 1280, 720 ) )
        .setWindowFlags( Configuration::WindowFlag::Resizable | ( IsDetached( arguments ) ? (Configuration::WindowFlag)0 : Configuration::WindowFlag::Maximized ) ),
        GLConfiguration{}
        .setSampleCount( 4 )
#endif
    },
    mIsDetachedNodeGraph( IsDetached( arguments ) ),
    mExecutablePath( arguments.argv[0] ),
    mIpcContext( fnEditorIpcSetup( false ) ),
    mImGuiIntegrationContext{ NoCreate },
    mImGuiContext{ ImGui::CreateContext() },
    mNodeEditor( *this )
{
    InitResources();

    const Vector2 size = Vector2 { windowSize() } / dpiScaling();

    // Add a font that actually looks acceptable on HiDPI screens.
    {
        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        const auto font = Utility::Resource{ "NodeEditor" }.getRaw( "Font.ttf" );
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<char*>( font.data() ), (int)font.size(), 14.0f * framebufferSize().x() / size.x(), &fontConfig );
    }

    // We manually save so we can sync the filesystem on emscripten
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::LoadIniSettingsFromDisk( kAppSettingsFile );

    ImGui::GetIO().ConfigDragClickToInputText = true;
    mImGuiIntegrationContext = ImGuiIntegration::Context( *mImGuiContext, size, windowSize(), framebufferSize() );
#ifndef CORRADE_TARGET_EMSCRIPTEN
    mImGuiIntegrationContext.connectApplicationClipboard(*this);
#endif

    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );

#ifndef __EMSCRIPTEN__
    setSwapInterval( 1 );
#endif

    mFrameTime.start();

    mCameraObject.setTransformation( Matrix4::translation( { 20, 20, 20 } ) );
    UpdatePespectiveProjection();

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation( GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add );
    GL::Renderer::setBlendFunction( GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha );

    Debug{} << "FastSIMD detected max CPU supported feature set:" << FastSIMD::GetFeatureSetString( FastSIMD::DetectCpuMaxFeatureSet() );

    mFeatureSetSelection = { FastSIMD::FeatureSet::Max };
    mFeatureSetSelection.insert( mFeatureSetSelection.end(),
        std::rbegin( FastSIMD::FastSIMD_FastNoise::CompiledFeatureSets::AsArray ), 
        std::rend( FastSIMD::FastSIMD_FastNoise::CompiledFeatureSets::AsArray ) );

    for( FastSIMD::FeatureSet featureSet : mFeatureSetSelection )
    {
        mFeatureSetNames.push_back( FastSIMD::GetFeatureSetString( featureSet ) );
    }
}

NodeEditorApp::~NodeEditorApp()
{
    // Avoid trying to save settings after node editor is already destroyed
    ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
    ImGui::GetIO().IniFilename = nullptr;

    fnEditorIpcRelease( mIpcContext );
}

void NodeEditorApp::SyncFileSystem()
{
#ifdef __EMSCRIPTEN__
    // Don't forget to sync to make sure you store it to IndexedDB
    EM_ASM(
        FS.syncfs( false, function( err ) {
            if (err) {
                console.warn("Error saving:", err);
            }
        } );
    );
#endif
}

void NodeEditorApp::drawEvent()
{
    GL::defaultFramebuffer.clear( GL::FramebufferClear::Color | GL::FramebufferClear::Depth );

    mImGuiIntegrationContext.newFrame();

    if( ImGui::GetIO().WantSaveIniSettings )
    {
        ImGui::SaveIniSettingsToDisk( kAppSettingsFile );
        ImGui::GetIO().WantSaveIniSettings = false;
        SyncFileSystem();
    }

    /* Enable text input, if needed */
    if( ImGui::GetIO().WantTextInput && !isTextInputActive() )
        startTextInput();
    else if( !ImGui::GetIO().WantTextInput && isTextInputActive() )
        stopTextInput();

    if( !mIsDetachedNodeGraph )
    {
        {
            if( ImGui::Button( "Reset State" ) )
            {
                ImGui::ClearIniSettings();
                mNodeEditor.~FastNoiseNodeEditor();
                new( &mNodeEditor ) FastNoiseNodeEditor( *this );
                ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
            }

            if( ImGui::ColorEdit3( "Clear Color", mClearColor.data() ) )
                GL::Renderer::setClearColor( mClearColor );

            ImGui::Checkbox( "Backface Culling", &mBackFaceCulling );

            ImGui::Text( "FastNoise2 v" FASTNOISE2_VERSION );

            ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)",
                         1000.0 / Double( ImGui::GetIO().Framerate ), Double( ImGui::GetIO().Framerate ) );

            if( ImGui::Combo( "Max Feature Set", &mMaxFeatureSet, mFeatureSetNames.data(), (int)mFeatureSetSelection.size() ) ||
                ImGuiExtra::ScrollCombo( &mMaxFeatureSet, (int)mFeatureSetSelection.size() ) )
            {
                FastSIMD::FeatureSet newLevel = mFeatureSetSelection[mMaxFeatureSet];
                mNodeEditor.SetSIMDLevel( newLevel );
            }
        }

        // Update camera pos
        Vector3 cameraVelocity( 0 );
        if( mKeyDown[Key_W] || mKeyDown[Key_Up] )
        {
            cameraVelocity.z() -= 1.0f;
        }
        if( mKeyDown[Key_S] || mKeyDown[Key_Down] )
        {
            cameraVelocity.z() += 1.0f;
        }
        if( mKeyDown[Key_A] || mKeyDown[Key_Left] )
        {
            cameraVelocity.x() -= 1.0f;
        }
        if( mKeyDown[Key_D] || mKeyDown[Key_Right] )
        {
            cameraVelocity.x() += 1.0f;
        }
        if( mKeyDown[Key_Q] || mKeyDown[Key_PgDn] )
        {
            cameraVelocity.y() -= 1.0f;
        }
        if( mKeyDown[Key_E] || mKeyDown[Key_PgUp] )
        {
            cameraVelocity.y() += 1.0f;
        }
        if( mKeyDown[Key_RShift] || mKeyDown[Key_LShift] )
        {
            cameraVelocity *= 4.0f;
        }

        cameraVelocity *= mFrameTime.previousFrameDuration() * 80.0f;

        if( !cameraVelocity.isZero() )
        {
            Matrix4 transform = mCameraObject.transformation();
            transform.translation() += transform.rotation() * cameraVelocity;
            mCameraObject.setTransformation( transform );
        }

        if( mBackFaceCulling )
        {
            GL::Renderer::enable( GL::Renderer::Feature::FaceCulling );
        }
    }

    mNodeEditor.Draw( mCamera.cameraMatrix(), mCamera.projectionMatrix(), mCameraObject.transformation().translation() );

    /* Set appropriate states. If you only draw ImGui, it is sufficient to
       just enable blending and scissor test in the constructor. */
    GL::Renderer::enable( GL::Renderer::Feature::Blending );
    GL::Renderer::enable( GL::Renderer::Feature::ScissorTest );
    GL::Renderer::disable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::disable( GL::Renderer::Feature::FaceCulling );

    mImGuiIntegrationContext.updateApplicationCursor( *this );
    mImGuiIntegrationContext.drawFrame();

    /* Reset state. Only needed if you want to draw something else with
       different state after. */
    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::disable( GL::Renderer::Feature::ScissorTest );
    GL::Renderer::disable( GL::Renderer::Feature::Blending );

    swapBuffers();
    redraw();
    mFrameTime.nextFrame();
}

void NodeEditorApp::viewportEvent( ViewportEvent& event )
{
    GL::defaultFramebuffer.setViewport( { {}, event.framebufferSize() } );

    UpdatePespectiveProjection();

    mImGuiIntegrationContext.relayout( Vector2 { event.windowSize() } / event.dpiScaling(), event.windowSize(), event.framebufferSize() );
}

void NodeEditorApp::keyPressEvent( KeyEvent& event )
{
    if( mImGuiIntegrationContext.handleKeyPressEvent( event ) )
        return;

    HandleKeyEvent( event.key(), true );
}

void NodeEditorApp::keyReleaseEvent( KeyEvent& event )
{
    if( mImGuiIntegrationContext.handleKeyReleaseEvent( event ) )
        return;

    HandleKeyEvent( event.key(), false );
}

void NodeEditorApp::HandleKeyEvent( Key key, bool value )
{
    switch( key )
    {
    case Key::W:
        mKeyDown[Key_W] = value;
        break;
    case Key::S:
        mKeyDown[Key_S] = value;
        break;
    case Key::A:
        mKeyDown[Key_A] = value;
        break;
    case Key::D:
        mKeyDown[Key_D] = value;
        break;
    case Key::Q:
        mKeyDown[Key_Q] = value;
        break;
    case Key::E:
        mKeyDown[Key_E] = value;
        break;
    case Key::Up:
        mKeyDown[Key_Up] = value;
        break;
    case Key::Down:
        mKeyDown[Key_Down] = value;
        break;
    case Key::Left:
        mKeyDown[Key_Left] = value;
        break;
    case Key::Right:
        mKeyDown[Key_Right] = value;
        break;
    case Key::PageDown:
        mKeyDown[Key_PgDn] = value;
        break;
    case Key::PageUp:
        mKeyDown[Key_PgUp] = value;
        break;
    case Key::LeftShift:
        mKeyDown[Key_LShift] = value;
        break;
    case Key::RightShift:
        mKeyDown[Key_RShift] = value;
        break;
    default:
        break;
    }
}

void NodeEditorApp::pointerPressEvent( PointerEvent& event )
{
    if( mImGuiIntegrationContext.handlePointerPressEvent( event ) )
        return;
    if( !event.isPrimary() || !(event.pointer() & Pointer::MouseLeft) )
        return;

    event.setAccepted();
}

void NodeEditorApp::pointerReleaseEvent( PointerEvent& event )
{
    if( mImGuiIntegrationContext.handlePointerReleaseEvent( event ) )
        return;

    event.setAccepted();
}

void NodeEditorApp::scrollEvent( ScrollEvent& event ) {
    if( mImGuiIntegrationContext.handleScrollEvent( event ) )
    {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void NodeEditorApp::pointerMoveEvent( PointerMoveEvent& event )
{
    if( mImGuiIntegrationContext.handlePointerMoveEvent( event ) )
        return;
    if( !event.isPrimary() || !(event.pointers() & Pointer::MouseLeft) )
        return;

    constexpr float mouseSensitivity = 0.22f;
    Vector2 angleDelta = Vector2( event.relativePosition() ) * mouseSensitivity;

    if( !angleDelta.isZero() ) 
    {    
        mLookAngle.x() = std::fmod( mLookAngle.x() - angleDelta.x(), 360.0f );
        mLookAngle.y() = std::clamp( mLookAngle.y() - angleDelta.y(), -89.f, 89.f );

        const Vector3 translation = mCameraObject.transformation().translation();
        const Matrix4 rotation = Matrix4::rotationY( Deg{ mLookAngle.x() } ) * Matrix4::rotationX( Deg{ mLookAngle.y() } );

        mCameraObject.setTransformation( Matrix4::lookAt( translation, translation - rotation.rotationNormalized() * Vector3::zAxis(), Vector3::yAxis() ) );
    }

    event.setAccepted();
}

void NodeEditorApp::textInputEvent( TextInputEvent& event )
{
    if( mImGuiIntegrationContext.handleTextInputEvent( event ) )
        return;
}

void NodeEditorApp::UpdatePespectiveProjection()
{
    mCamera.setProjectionMatrix( Matrix4::perspectiveProjection( Deg( 70.0f ), Vector2{ windowSize() }.aspectRatio(), 2.0f, 3500.0f ) );
}


MAGNUM_APPLICATION_MAIN( NodeEditorApp )
