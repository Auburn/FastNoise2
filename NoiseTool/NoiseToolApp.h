#pragma once

#include <array>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GlfwApplication.h>
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
        ~NoiseToolApp();

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
        void HandleKeyEvent( KeyEvent::Key key, bool value );

        SceneGraph::Object<SceneGraph::MatrixTransformation3D> mCameraObject;
        SceneGraph::Camera3D mCamera{ mCameraObject };
        Vector2 mLookAngle{ 0 };
        Timeline mFrameTime;

        Color3 mClearColor{ 0.122f };
        bool mBackFaceCulling = false;
        int mMaxSIMDLevel = 0;
        std::vector<const char*> mLevelNames;
        std::vector<FastSIMD::eLevel> mLevelEnums;

        ImGuiIntegration::Context mImGuiIntegrationContext;
        ImGuiContext* mImGuiContext;
        FastNoiseNodeEditor mNodeEditor;

        enum Key
        {
            Key_W, Key_A, Key_S, Key_D, Key_Q, Key_E,
            Key_Left, Key_Right, Key_Up, Key_Down, Key_PgUp, Key_PgDn,
            Key_LShift, Key_RShift,
            Key_Count
        };

        std::array<bool, Key_Count> mKeyDown = {};
    };
}
