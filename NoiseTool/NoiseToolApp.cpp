#include "NoiseToolApp.h"

using namespace Magnum;

NoiseToolApp::NoiseToolApp(const Arguments& arguments) :
    Platform::Application{ arguments, Configuration{}.setTitle("FastNoise Tool") }
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
            Deg( 35.0f ), Vector2{ windowSize() }.aspectRatio(), 0.01f, 100.0f)*
        Matrix4::translation(Vector3::zAxis(-10.0f));
    _color = Color3::fromHsv(Deg(35.0f), 1.0f, 1.0f);
}

void NoiseToolApp::drawEvent() {
    GL::defaultFramebuffer.clear(
        GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    _shader.setLightPosition({ 7.0f, 5.0f, 2.5f })
        .setLightColor(Color3{ 1.0f })
        .setDiffuseColor(_color)
        .setAmbientColor(Color3::fromHsv(_color.hue(), 1.0f, 0.3f))
        .setTransformationMatrix(_transformation)
        .setNormalMatrix(_transformation.rotationScaling())
        .setProjectionMatrix(_projection);
    _mesh.draw(_shader);

    swapBuffers();
}

void NoiseToolApp::mousePressEvent(MouseEvent& event) {
    if (event.button() != MouseEvent::Button::Left) return;

    _previousMousePosition = event.position();
    event.setAccepted();
}

void NoiseToolApp::mouseReleaseEvent(MouseEvent& event) {
    _color = Color3::fromHsv(_color.hue() + Deg( 50.0f ), 1.0f, 1.0f);

    event.setAccepted();
    redraw();
}

void NoiseToolApp::mouseMoveEvent(MouseMoveEvent& event) {
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
