#pragma once

#include <array>

#ifdef __EMSCRIPTEN__
#define FILESYSTEM_ROOT "/fastnoise2/"
#include <Magnum/Platform/EmscriptenApplication.h>
#else
#define FILESYSTEM_ROOT
#include <Magnum/Platform/GlfwApplication.h>
#endif

#include <Magnum/Math/Color.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>

#include "FastNoiseNodeEditor.h"

namespace Magnum
{
    class NodeEditorApp : public Platform::Application
    {
    public:
        explicit NodeEditorApp( const Arguments& arguments );
        ~NodeEditorApp();

        bool IsDetachedNodeGraph()
        {
            return mIsDetachedNodeGraph;
        }

        void* GetIpcSharedMemory()
        {
            return mIpcSharedMemory;
        }

        std::string_view GetExecutablePath()
        {
            return mExecutablePath;
        }

        static void SyncFileSystem();

    private:
        void drawEvent() override;
        void viewportEvent( ViewportEvent& event ) override;

        void keyPressEvent( KeyEvent& event ) override;
        void keyReleaseEvent( KeyEvent& event ) override;
        void pointerPressEvent( PointerEvent& event ) override;
        void pointerReleaseEvent( PointerEvent& event ) override;
        void pointerMoveEvent( PointerMoveEvent& event ) override;
        void scrollEvent( ScrollEvent& event ) override;
        void textInputEvent( TextInputEvent& event ) override;

        void UpdatePespectiveProjection();
        void HandleKeyEvent( Key key, bool value );

        bool mIsDetachedNodeGraph;
        std::string mExecutablePath;
        void* mIpcSharedMemory;

        SceneGraph::Object<SceneGraph::MatrixTransformation3D> mCameraObject;
        SceneGraph::Camera3D mCamera{ mCameraObject };
        Vector2 mLookAngle{ 0 };
        Timeline mFrameTime;

        Color3 mClearColor{ 0.122f };
        bool mBackFaceCulling = false;
        int mMaxFeatureSet = 0;
        std::vector<FastSIMD::FeatureSet> mFeatureSetSelection;
        std::vector<const char*> mFeatureSetNames;

        ImGuiIntegration::Context mImGuiIntegrationContext;
        ImGuiContext* mImGuiContext;
        FastNoiseNodeEditor mNodeEditor;

        enum KeyMap
        {
            Key_W, Key_A, Key_S, Key_D, Key_Q, Key_E,
            Key_Left, Key_Right, Key_Up, Key_Down, Key_PgUp, Key_PgDn,
            Key_LShift, Key_RShift,
            Key_Count
        };

        std::array<bool, Key_Count> mKeyDown = {};
    };
}
