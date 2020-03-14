#include "NoiseToolApp.h"
#include <imgui.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/ImageView.h>

#include "FastNoise/FastNoise.h"
#include "imnodes.h"

using namespace Magnum;

NoiseToolApp::NoiseToolApp(const Arguments& arguments) :
    Platform::Application{ arguments, Configuration{}
    .setTitle("FastNoise Tool")
    .setSize( Vector2i( 1920, 1080 ) )
    .setWindowFlags( Sdl2Application::Configuration::WindowFlag::Resizable )
    },
    mNoiseImage(PixelFormat::R32F)
{
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    const Trade::MeshData3D cube = Primitives::cubeSolid();

    _vertexBuffer.setData(MeshTools::interleave(cube.positions(0), cube.normals(0)));

    Containers::Array<char> indexData;
    MeshIndexType indexType;
    UnsignedInt indexStart, indexEnd;
    std::tie(indexData, indexType, indexStart, indexEnd) =
        MeshTools::compressIndices(cube.indices());
    _indexBuffer.setData(indexData);

    _mesh.setPrimitive(cube.primitive())
        .setCount(cube.indices().size())
        .addVertexBuffer(_vertexBuffer, 0, Shaders::Phong::Position{},
            Shaders::Phong::Normal{})
        .setIndexBuffer(_indexBuffer, 0, indexType, indexStart, indexEnd);

    _transformation =
        Matrix4::rotationX(Deg( 30.0f ))*Matrix4::rotationY(Deg( 40.0f ));
    
    _projection =
        Matrix4::perspectiveProjection(
            Deg(35.0f), Vector2{ windowSize() }.aspectRatio(), 0.01f, 100.0f)*
        Matrix4::translation(Vector3::zAxis(-10.0f));

    _color = Color3::fromHsv( Math::ColorHsv( Deg(35.0f), 1.0f, 1.0f ));

    _imgui = ImGuiIntegration::Context(Vector2{ windowSize() } / dpiScaling(),
        windowSize(), framebufferSize());

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add,
        GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
        GL::Renderer::BlendFunction::OneMinusSourceAlpha);


    // FastNoise
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource( FastNoise::New<FastNoise::Simplex>() );

    auto generator = FastNoise::New<FastNoise::ConvertRGBA8>();
    generator->SetSource( fractal );
    
    //std::cout << "SIMD Level:  " << generator->GetSIMDLevel() << "\n";
    
    Containers::Array<char> data(512 * 512 * sizeof(float));

    Containers::Array<float> x(512 * 512);
    Containers::Array<float> y(512 * 512);
    Containers::Array<float> z(512 * 512);

    for (int i = 0; i < 512 * 512; i++)
    {
        x[i] = 0;
    }
    for (int i = 0; i < 512 * 512; i++)
    {
        y[i] = (i % 512) * 0.01f;
    }
    for (int i = 0; i < 512 * 512; i++)
    {
        z[i] = (i / 512) * 0.01f;
    }

    //generator->GenPositionArray3D((float*)data.data(), x, y, z, 512 * 512, 0, 0, 0, 1337);

    generator->GenUniformGrid2D((float*)data.data(), 0, 0, 512, 512, 0.01f, 0.01f, 1337 );

    mNoiseImage = Image2D( PixelFormat::RGBA8Unorm, { 512, 512 }, std::move(data) );
    
    mNoiseTexture.setMagnificationFilter(GL::SamplerFilter::Linear)
        .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
        .setStorage(Math::log2(512) + 1, GL::TextureFormat::RGBA8, { 512, 512 })
        .setSubImage(0, {}, mNoiseImage)
        .generateMipmap();


    imnodes::Initialize();

    // Setup style
    //imnodes::SetNodeGridSpacePos(1, ImVec2(200.0f, 200.0f));
}

void NoiseToolApp::drawEvent() {
    GL::defaultFramebuffer.clear(
        GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    _shader.setLightPosition({ 7.0f, 5.0f, 2.5f })
        .setLightColor(Color3{ 1.0f })
        .setDiffuseColor(_color)
        .setAmbientColor(Color3::fromHsv(Math::ColorHsv(_color.hue(), 1.0f, 0.3f)))
        .setTransformationMatrix(_transformation)
        .setNormalMatrix(_transformation.rotationScaling())
        .setProjectionMatrix(_projection);
    _mesh.draw(_shader);

    _imgui.newFrame();

    /* Enable text input, if needed */
    if (ImGui::GetIO().WantTextInput && !isTextInputActive())
        startTextInput();
    else if (!ImGui::GetIO().WantTextInput && isTextInputActive())
        stopTextInput();

    /* 1. Show a simple window.
       Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appear in
       a window called "Debug" automatically */
    {
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("Float", &_floatValue, 0.0f, 1.0f);
        if (ImGui::ColorEdit3("Clear Color", _clearColor.data()))
            GL::Renderer::setClearColor(_clearColor);
        if (ImGui::Button("Test Window"))
        {
        }
        if (ImGui::Button("Another Window"))
        {
        }
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0 / Double(ImGui::GetIO().Framerate), Double(ImGui::GetIO().Framerate));


        /*ImGui::Begin("Node Editor");
        imnodes::BeginNodeEditor();
        imnodes::BeginNode(1);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("simple node :)");
        imnodes::EndNodeTitleBar();


        imnodes::BeginInputAttribute(2);
        ImGui::Text("input");
        imnodes::EndAttribute();

        imnodes::BeginOutputAttribute(3);
        ImGuiIntegration::image(mNoiseTexture, {128,128});
        imnodes::EndAttribute();

        imnodes::EndNode();
        imnodes::EndNodeEditor();
        ImGui::End();*/
    }

    mNodeEditor.Update();

    /* Set appropriate states. If you only draw ImGui, it is sufficient to
       just enable blending and scissor test in the constructor. */
    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    _imgui.drawFrame();

    /* Reset state. Only needed if you want to draw something else with
       different state after. */
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::Blending);

    swapBuffers();
    redraw();
}

void NoiseToolApp::viewportEvent(ViewportEvent& event) {
    GL::defaultFramebuffer.setViewport({ {}, event.framebufferSize() });

    _projection =
        Matrix4::perspectiveProjection(
            Deg(35.0f), Vector2{ windowSize() }.aspectRatio(), 0.01f, 100.0f)*
        Matrix4::translation(Vector3::zAxis(-10.0f));

    _imgui.relayout(Vector2{ event.windowSize() } / event.dpiScaling(),
        event.windowSize(), event.framebufferSize());
}

void NoiseToolApp::keyPressEvent(KeyEvent& event) {
    if (_imgui.handleKeyPressEvent(event)) return;
}

void NoiseToolApp::keyReleaseEvent(KeyEvent& event) {
    if (_imgui.handleKeyReleaseEvent(event)) return;
}

void NoiseToolApp::mousePressEvent(MouseEvent& event) {
    if (_imgui.handleMousePressEvent(event)) return;
    if (event.button() != MouseEvent::Button::Left) return;

    _previousMousePosition = event.position();
    event.setAccepted();
}

void NoiseToolApp::mouseReleaseEvent(MouseEvent& event) {
    if (_imgui.handleMouseReleaseEvent(event)) return;
    _color = Color3::fromHsv(Math::ColorHsv((Deg)(_color.hue() + Deg( 50.0f )), 1.0f, 1.0f));

    event.setAccepted();
    redraw();
}

void NoiseToolApp::mouseScrollEvent(MouseScrollEvent& event) {
    if(_imgui.handleMouseScrollEvent(event)) {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void NoiseToolApp::mouseMoveEvent(MouseMoveEvent& event) {
    if (_imgui.handleMouseMoveEvent(event)) return;
    if (!(event.buttons() & MouseMoveEvent::Button::Left)) return;

    const Vector2 delta = 3.0f*
        Vector2{ event.position() - _previousMousePosition } /
        Vector2{ GL::defaultFramebuffer.viewport().size() };

    _transformation =
        Matrix4::rotationX(Rad{ delta.y() })*
        _transformation*
        Matrix4::rotationY(Rad{ delta.x() });

    _previousMousePosition = event.position();
    event.setAccepted();
    redraw();
}

void NoiseToolApp::textInputEvent(TextInputEvent& event) {
    if(_imgui.handleTextInputEvent(event)) return;
}

MAGNUM_APPLICATION_MAIN( NoiseToolApp )