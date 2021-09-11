#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#include "NoiseToolApp.h"
#include "ImGuiExtra.h"

using namespace Magnum;

void InitResources()
{
#ifdef MAGNUM_BUILD_STATIC
    CORRADE_RESOURCE_INITIALIZE( NoiseTool_RESOURCES )
#endif
}

NoiseToolApp::NoiseToolApp( const Arguments& arguments ) :
    Platform::Application{ arguments,
    Configuration{}
    .setTitle( "FastNoise2 NoiseTool" )
    .setSize( Vector2i( 1280, 720 ) )
    .setWindowFlags( Configuration::WindowFlag::Resizable | Configuration::WindowFlag::Maximized ),
    GLConfiguration{}
    .setSampleCount( 4 )
    },
    mImGuiIntegrationContext{ NoCreate },
    mImGuiContext{ ImGui::CreateContext() }
{
    InitResources();

    const Vector2 size = Vector2 { windowSize() } / dpiScaling();

    // Add a font that actually looks acceptable on HiDPI screens.
    {
        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;
        const auto font = Utility::Resource{ "NoiseTool" }.getRaw( "Font.ttf" );
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<char*>( font.data() ), (int)font.size(), 14.0f * framebufferSize().x() / size.x(), &fontConfig );
    }

    ImGui::GetIO().IniFilename = "NoiseTool.ini";
    mImGuiIntegrationContext = ImGuiIntegration::Context( *mImGuiContext, size, windowSize(), framebufferSize() );

    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );

    setSwapInterval( 1 );

    mFrameTime.start();

    mCameraObject.setTransformation( Matrix4::translation( { 20, 20, 20 } ) );
    UpdatePespectiveProjection();

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation( GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add );
    GL::Renderer::setBlendFunction( GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha );

    Debug{} << "FastSIMD detected max CPU SIMD Level:" << FastNoiseNodeEditor::GetSIMDLevelName( FastSIMD::CPUMaxSIMDLevel() );

    mLevelNames = { "Auto" };
    mLevelEnums = { FastSIMD::Level_Null };

    for( int i = 1; i > 0; i <<= 1 )
    {
        FastSIMD::eLevel lvl = (FastSIMD::eLevel)i;
        if( lvl & FastNoise::SUPPORTED_SIMD_LEVELS & FastSIMD::COMPILED_SIMD_LEVELS )
        {
            mLevelNames.emplace_back( FastNoiseNodeEditor::GetSIMDLevelName( lvl ) );
            mLevelEnums.emplace_back( lvl );
        }
    }
}

NoiseToolApp::~NoiseToolApp()
{
    // Avoid trying to save settings after node editor is already destroyed
    ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
    ImGui::GetIO().IniFilename = nullptr;
}

void NoiseToolApp::drawEvent()
{
    GL::defaultFramebuffer.clear( GL::FramebufferClear::Color | GL::FramebufferClear::Depth );

    mImGuiIntegrationContext.newFrame();

    /* Enable text input, if needed */
    if( ImGui::GetIO().WantTextInput && !isTextInputActive() )
        startTextInput();
    else if( !ImGui::GetIO().WantTextInput && isTextInputActive() )
        stopTextInput();

    {
        if( ImGui::Button( "Reset State" ) )
        {
            ImGui::ClearIniSettings();
            mNodeEditor.~FastNoiseNodeEditor();
            new( &mNodeEditor ) FastNoiseNodeEditor();
            ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
        }

        if( ImGui::ColorEdit3( "Clear Color", mClearColor.data() ) )
            GL::Renderer::setClearColor( mClearColor );

        ImGui::Checkbox( "Backface Culling", &mBackFaceCulling );

        ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)",
            1000.0 / Double( ImGui::GetIO().Framerate ), Double( ImGui::GetIO().Framerate ) );

        if( ImGui::Combo( "Max SIMD Level", &mMaxSIMDLevel, mLevelNames.data(), (int)mLevelEnums.size() ) ||
            ImGuiExtra::ScrollCombo( &mMaxSIMDLevel, (int)mLevelEnums.size() ) )
        {   
            FastSIMD::eLevel newLevel = mLevelEnums[mMaxSIMDLevel];
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

void NoiseToolApp::viewportEvent( ViewportEvent& event )
{
    GL::defaultFramebuffer.setViewport( { {}, event.framebufferSize() } );

    UpdatePespectiveProjection();

    mImGuiIntegrationContext.relayout( Vector2 { event.windowSize() } / event.dpiScaling(), event.windowSize(), event.framebufferSize() );
}

void NoiseToolApp::keyPressEvent( KeyEvent& event )
{
    if( mImGuiIntegrationContext.handleKeyPressEvent( event ) )
        return;

    HandleKeyEvent( event.key(), true );
}

void NoiseToolApp::keyReleaseEvent( KeyEvent& event )
{
    if( mImGuiIntegrationContext.handleKeyReleaseEvent( event ) )
        return;

    HandleKeyEvent( event.key(), false );
}

void NoiseToolApp::HandleKeyEvent( KeyEvent::Key key, bool value )
{
    switch( key )
    {
    case KeyEvent::Key::W:
        mKeyDown[Key_W] = value;
        break;
    case KeyEvent::Key::S:
        mKeyDown[Key_S] = value;
        break;
    case KeyEvent::Key::A:
        mKeyDown[Key_A] = value;
        break;
    case KeyEvent::Key::D:
        mKeyDown[Key_D] = value;
        break;
    case KeyEvent::Key::Q:
        mKeyDown[Key_Q] = value;
        break;
    case KeyEvent::Key::E:
        mKeyDown[Key_E] = value;
        break;
    case KeyEvent::Key::Up:
        mKeyDown[Key_Up] = value;
        break;
    case KeyEvent::Key::Down:
        mKeyDown[Key_Down] = value;
        break;
    case KeyEvent::Key::Left:
        mKeyDown[Key_Left] = value;
        break;
    case KeyEvent::Key::Right:
        mKeyDown[Key_Right] = value;
        break;
    case KeyEvent::Key::PageDown:
        mKeyDown[Key_PgDn] = value;
        break;
    case KeyEvent::Key::PageUp:
        mKeyDown[Key_PgUp] = value;
        break;
    case KeyEvent::Key::LeftShift:
        mKeyDown[Key_LShift] = value;
        break;
    case KeyEvent::Key::RightShift:
        mKeyDown[Key_RShift] = value;
        break;
    default:
        break;
    }
}

void NoiseToolApp::mousePressEvent( MouseEvent& event )
{
    if( mImGuiIntegrationContext.handleMousePressEvent( event ) )
        return;
    if( event.button() != MouseEvent::Button::Left )
        return;

    event.setAccepted();
}

void NoiseToolApp::mouseReleaseEvent( MouseEvent& event )
{
    if( mImGuiIntegrationContext.handleMouseReleaseEvent( event ) )
        return;

    event.setAccepted();
}

void NoiseToolApp::mouseScrollEvent( MouseScrollEvent& event ) {
    if( mImGuiIntegrationContext.handleMouseScrollEvent( event ) )
    {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void NoiseToolApp::mouseMoveEvent( MouseMoveEvent& event )
{
    if( mImGuiIntegrationContext.handleMouseMoveEvent( event ) )
        return;
    if( !(event.buttons() & MouseMoveEvent::Button::Left) )
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

void NoiseToolApp::textInputEvent( TextInputEvent& event )
{
    if( mImGuiIntegrationContext.handleTextInputEvent( event ) )
        return;
}

void NoiseToolApp::UpdatePespectiveProjection()
{
    mCamera.setProjectionMatrix( Matrix4::perspectiveProjection( Deg( 70.0f ), Vector2{ windowSize() }.aspectRatio(), 2.0f, 3500.0f ) );
}


MAGNUM_APPLICATION_MAIN( NoiseToolApp )
