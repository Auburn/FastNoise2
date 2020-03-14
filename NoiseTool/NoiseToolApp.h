#pragma once

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Image.h>
#include <Corrade/Containers/StaticArray.h>
#include <Magnum/ImGuiIntegration/Context.h>

#include "FastNoiseNodeEditor.h"

namespace Magnum
{
    class NoiseToolApp : public Platform::Application
    {
    public:
        explicit NoiseToolApp(const Arguments& arguments);

    private:
        void drawEvent() override;
        void viewportEvent(ViewportEvent& event) override;

        void keyPressEvent(KeyEvent& event) override;
        void keyReleaseEvent(KeyEvent& event) override;
        void mousePressEvent(MouseEvent& event) override;
        void mouseReleaseEvent(MouseEvent& event) override;
        void mouseMoveEvent(MouseMoveEvent& event) override;
        void mouseScrollEvent(MouseScrollEvent& event) override;
        void textInputEvent(TextInputEvent& event) override;

        GL::Buffer _indexBuffer, _vertexBuffer;
        GL::Mesh _mesh;
        Shaders::Phong _shader;

        Matrix4 _transformation, _projection;
        Vector2i _previousMousePosition;
        Color3 _color;

        ImGuiIntegration::Context _imgui{ NoCreate }; 
        Color4 _clearColor = Color4( 0x72, 0x90, 0x9a );
        Float _floatValue = 0.0f;

        GL::Texture2D mNoiseTexture;
        Image2D mNoiseImage;

        FastNoiseNodeEditor mNodeEditor;
    };
}