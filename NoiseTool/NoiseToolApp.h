#pragma once

#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>

#include "FastNoiseNodeEditor.h"

namespace Magnum
{
    class NoiseToolApp : public Platform::Application
    {
    public:
        explicit NoiseToolApp( const Arguments& arguments );

    private:
        void drawEvent() override;
        void viewportEvent( ViewportEvent& event ) override;

        void keyPressEvent( KeyEvent& event ) override;
        void keyReleaseEvent( KeyEvent& event ) override;
        void mousePressEvent( MouseEvent& event ) override;
        void mouseReleaseEvent( MouseEvent& event ) override;
        void mouseMoveEvent( MouseMoveEvent& event ) override;
        void mouseScrollEvent( MouseScrollEvent& event ) override;
        void textInputEvent( TextInputEvent& event ) override;

        void UpdatePespectiveProjection();

        SceneGraph::Object<SceneGraph::MatrixTransformation3D> mCameraObject;
        SceneGraph::Camera3D mCamera{ mCameraObject };
        Vector3 mCameraVelocity;

        ImGuiIntegration::Context mImGuiContext{ NoCreate };
        Color3 mClearColor{ 0.122f };

        FastNoiseNodeEditor mNodeEditor;
    };
}
