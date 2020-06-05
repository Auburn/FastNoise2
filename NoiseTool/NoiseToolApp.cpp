#include <imgui.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/GL/Renderer.h>

#include "NoiseToolApp.h"
#include "FastNoise/FastNoise.h"

using namespace Magnum;

NoiseToolApp::NoiseToolApp( const Arguments& arguments ) :
    Platform::Application{ arguments, Configuration{}
    .setTitle( "FastNoise Tool" )
    .setSize( Vector2i( 1920, 1080 ) )
    .setWindowFlags( Configuration::WindowFlag::Resizable ) }
{
    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::enable( GL::Renderer::Feature::FaceCulling );

    mCameraObject.setTransformation( Matrix4::translation( Vector3::zAxis( 50.0f ) ) );
    mFrameTime.start();

    UpdatePespectiveProjection();

    mImGuiContext = ImGuiIntegration::Context( Vector2{ windowSize() } / dpiScaling(), windowSize(), framebufferSize() );

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation( GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add );
    GL::Renderer::setBlendFunction( GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha );
}

void NoiseToolApp::drawEvent()
{
    GL::defaultFramebuffer.clear( GL::FramebufferClear::Color | GL::FramebufferClear::Depth );

    mImGuiContext.newFrame();

    /* Enable text input, if needed */
    if( ImGui::GetIO().WantTextInput && !isTextInputActive() )
        startTextInput();
    else if( !ImGui::GetIO().WantTextInput && isTextInputActive() )
        stopTextInput();

    {
        if( ImGui::ColorEdit3( "Clear Color", mClearColor.data() ) )
            GL::Renderer::setClearColor( mClearColor );

        ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)",
            1000.0 / Double( ImGui::GetIO().Framerate ), Double( ImGui::GetIO().Framerate ) );
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
        cameraVelocity *= 3.0f;
    }

    cameraVelocity *= mFrameTime.previousFrameDuration() * 100.0f;

    if( !cameraVelocity.isZero() ) {
        Matrix4 transform = mCameraObject.transformation();
        transform.translation() += transform.rotation() * cameraVelocity;
        mCameraObject.setTransformation( transform );
        redraw();
    }

    mNodeEditor.Draw( mCamera.cameraMatrix(), mCamera.projectionMatrix() );

    /* Set appropriate states. If you only draw ImGui, it is sufficient to
       just enable blending and scissor test in the constructor. */
    GL::Renderer::enable( GL::Renderer::Feature::Blending );
    GL::Renderer::enable( GL::Renderer::Feature::ScissorTest );
    GL::Renderer::disable( GL::Renderer::Feature::FaceCulling );
    GL::Renderer::disable( GL::Renderer::Feature::DepthTest );

    mImGuiContext.drawFrame();

    /* Reset state. Only needed if you want to draw something else with
       different state after. */
    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::enable( GL::Renderer::Feature::FaceCulling );
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

    mImGuiContext.relayout( Vector2{ event.windowSize() } / event.dpiScaling(), event.windowSize(), event.framebufferSize() );
}

void NoiseToolApp::keyPressEvent( KeyEvent& event )
{
    if( mImGuiContext.handleKeyPressEvent( event ) ) return;

    HandleKeyEvent( event.key(), true );
}

void NoiseToolApp::keyReleaseEvent( KeyEvent& event )
{
    if( mImGuiContext.handleKeyReleaseEvent( event ) ) return;

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
    if( mImGuiContext.handleMousePressEvent( event ) ) return;
    if( event.button() != MouseEvent::Button::Left ) return;

    event.setAccepted();
}

void NoiseToolApp::mouseReleaseEvent( MouseEvent& event )
{
    if( mImGuiContext.handleMouseReleaseEvent( event ) ) return;

    event.setAccepted();
}

void NoiseToolApp::mouseScrollEvent( MouseScrollEvent& event ) {
    if( mImGuiContext.handleMouseScrollEvent( event ) )
    {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void NoiseToolApp::mouseMoveEvent( MouseMoveEvent& event )
{
    if( mImGuiContext.handleMouseMoveEvent( event ) ) return;
    if( !(event.buttons() & MouseMoveEvent::Button::Left) ) return;

    const Matrix4 transform = mCameraObject.transformation();

    constexpr float angleScale = 0.004f;
    float angleX = event.relativePosition().x() * angleScale;
    float angleY = event.relativePosition().y() * angleScale;

    if( angleX != 0.0f || angleY != 0.0f ) 
    {
        mCameraObject.setTransformation( Matrix4::lookAt( transform.translation(),
            transform.translation() - transform.rotationScaling() * Vector3 { -angleX, angleY, 1.0f },
            Vector3::yAxis() ) );
    }

    event.setAccepted();
    redraw();
}

void NoiseToolApp::textInputEvent( TextInputEvent& event )
{
    if( mImGuiContext.handleTextInputEvent( event ) ) return;
}

void NoiseToolApp::UpdatePespectiveProjection()
{
    mCamera.setProjectionMatrix( Matrix4::perspectiveProjection( Deg( 70.0f ), Vector2{ windowSize() }.aspectRatio(), 0.01f, 1000.0f ) );
}


MAGNUM_APPLICATION_MAIN( NoiseToolApp )
