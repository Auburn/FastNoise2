#ifndef Magnum_ImGuiIntegration_Context_hpp
#define Magnum_ImGuiIntegration_Context_hpp
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 ShaddyAQN <ShaddyAQN@gmail.com>
    Copyright © 2018 Tomáš Skřivan <skrivantomas@seznam.cz>
    Copyright © 2018 Jonathan Hale <squareys@googlemail.com>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Template method implementations for input handling methods in @ref Magnum::ImGuiIntegration::Context
 */

#include "Magnum/ImGuiIntegration/visibility.h" /* defines IMGUI_API */

#include <imgui.h>
#include <Corrade/Utility/Resource.h>

#include "Magnum/ImGuiIntegration/Integration.h"
#include "Magnum/ImGuiIntegration/Context.h"

namespace Magnum { namespace ImGuiIntegration {

template<class KeyEvent> bool Context::handleKeyEvent(KeyEvent& event, bool value) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    ImGuiIO &io = ImGui::GetIO();

    switch(event.key()) {
        /* LCOV_EXCL_START */
        case KeyEvent::Key::LeftShift:
        case KeyEvent::Key::RightShift:
            io.KeyShift = value;
            break;
        case KeyEvent::Key::LeftCtrl:
        case KeyEvent::Key::RightCtrl:
            io.KeyCtrl = value;
            break;
        case KeyEvent::Key::LeftAlt:
        case KeyEvent::Key::RightAlt:
            io.KeyAlt = value;
            break;
        case KeyEvent::Key::LeftSuper:
        case KeyEvent::Key::RightSuper:
            io.KeySuper = value;
            break;
        case KeyEvent::Key::Tab:
            io.KeysDown[ImGuiKey_Tab] = value;
            break;
        case KeyEvent::Key::Up:
            io.KeysDown[ImGuiKey_UpArrow] = value;
            break;
        case KeyEvent::Key::Down:
            io.KeysDown[ImGuiKey_DownArrow] = value;
            break;
        case KeyEvent::Key::Left:
            io.KeysDown[ImGuiKey_LeftArrow] = value;
            break;
        case KeyEvent::Key::Right:
            io.KeysDown[ImGuiKey_RightArrow] = value;
            break;
        case KeyEvent::Key::Home:
            io.KeysDown[ImGuiKey_Home] = value;
            break;
        case KeyEvent::Key::End:
            io.KeysDown[ImGuiKey_End] = value;
            break;
        case KeyEvent::Key::PageUp:
            io.KeysDown[ImGuiKey_PageUp] = value;
            break;
        case KeyEvent::Key::PageDown:
            io.KeysDown[ImGuiKey_PageDown] = value;
            break;
        case KeyEvent::Key::Enter:
        case KeyEvent::Key::NumEnter:
            io.KeysDown[ImGuiKey_Enter] = value;
            break;
        case KeyEvent::Key::Esc:
            io.KeysDown[ImGuiKey_Escape] = value;
            break;
        case KeyEvent::Key::Space:
            io.KeysDown[ImGuiKey_Space] = value;
            break;
        case KeyEvent::Key::Backspace:
            io.KeysDown[ImGuiKey_Backspace] = value;
            break;
        case KeyEvent::Key::Delete:
            io.KeysDown[ImGuiKey_Delete] = value;
            break;
        case KeyEvent::Key::A:
            io.KeysDown[ImGuiKey_A] = value;
            break;
        case KeyEvent::Key::C:
            io.KeysDown[ImGuiKey_C] = value;
            break;
        case KeyEvent::Key::V:
            io.KeysDown[ImGuiKey_V] = value;
            break;
        case KeyEvent::Key::X:
            io.KeysDown[ImGuiKey_X] = value;
            break;
        case KeyEvent::Key::Y:
            io.KeysDown[ImGuiKey_Y] = value;
            break;
        case KeyEvent::Key::Z:
            io.KeysDown[ImGuiKey_Z] = value;
            break;
        /* LCOV_EXCL_STOP */

        /* Unknown key, do nothing */
        default: return false;
    }

    return io.WantCaptureKeyboard;
}

template<class MouseEvent> bool Context::handleMouseEvent(MouseEvent& event, bool value) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(Vector2(event.position())*_eventScaling);

    std::size_t buttonId;
    switch(event.button()) {
        case MouseEvent::Button::Left:
            buttonId = 0;
            break;
        case MouseEvent::Button::Right:
            buttonId = 1;
            break;
        case MouseEvent::Button::Middle:
            buttonId = 2;
            break;

        /* Unknown button, do nothing */
        default: return false;
    }

    /* Instead of setting io.MouseDown directly, we delay this until the
       newFrame() call in order to prevent mouse clicks from being ignored when
       both a press and a release happens in the same frame. Apart from this
       happening when the app can't render fast enough, for some reason it also
       happens with SDL2 on macOS -- press delayed by a significant  */
    _mousePressed.set(buttonId, value);
    if(value) _mousePressedInThisFrame.set(buttonId, true);

    return io.WantCaptureMouse;
}

template<class MouseEvent> bool Context::handleMousePressEvent(MouseEvent& event) {
    return handleMouseEvent(event, true);
}

template<class MouseEvent> bool Context::handleMouseReleaseEvent(MouseEvent& event) {
    return handleMouseEvent(event, false);
}

template<class MouseScrollEvent> bool Context::handleMouseScrollEvent(MouseScrollEvent& event) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(Vector2(event.position())*_eventScaling);
    io.MouseWheel += event.offset().y();
    io.MouseWheelH += event.offset().x();
    return io.WantCaptureMouse;
}

template<class MouseMoveEvent> bool Context::handleMouseMoveEvent(MouseMoveEvent& event) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(Vector2(event.position())*_eventScaling);
    return io.WantCaptureMouse;
}

template<class KeyEvent> bool Context::handleKeyPressEvent(KeyEvent& event) {
    return handleKeyEvent(event, true);
}

template<class KeyEvent> bool Context::handleKeyReleaseEvent(KeyEvent& event) {
    return handleKeyEvent(event, false);
}

template<class TextInputEvent> bool Context::handleTextInputEvent(TextInputEvent& event) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    ImGui::GetIO().AddInputCharactersUTF8(event.text().data());
    return false;
}

/* GLFW doesn't have all cursor types, so employing a dirty SFINAE trick that
   falls back to Arrow if given enum value is not available */
namespace Implementation {

#ifndef DOXYGEN_GENERATING_OUTPUT
#define MAGNUM_IMGUIINTEGRATION_OPTIONAL_CURSOR(cursor)                     \
    template<class U> struct Optional##cursor##Cursor {                     \
        template<class T> constexpr static typename T::Cursor get(T*, decltype(T::Cursor::cursor)* = nullptr) { return T::Cursor::cursor; } \
        constexpr static typename U::Cursor get(...) { return U::Cursor::Arrow; } \
        public:                                                             \
            constexpr static typename U::Cursor Cursor = get(static_cast<U*>(nullptr)); \
    };
MAGNUM_IMGUIINTEGRATION_OPTIONAL_CURSOR(ResizeAll)
MAGNUM_IMGUIINTEGRATION_OPTIONAL_CURSOR(ResizeNESW)
MAGNUM_IMGUIINTEGRATION_OPTIONAL_CURSOR(ResizeNWSE)
#undef MAGNUM_IMGUIINTEGRATION_OPTIONAL_CURSOR
#endif

}

template<class Application> void Context::updateApplicationCursor(Application& application) {
    /* Ensure we use the context we're linked to */
    ImGui::SetCurrentContext(_context);

    switch(ImGui::GetMouseCursor()) {
        case ImGuiMouseCursor_TextInput:
            application.setCursor(Application::Cursor::TextInput);
            return;
        case ImGuiMouseCursor_ResizeAll:
            application.setCursor(Implementation::OptionalResizeAllCursor<Application>::Cursor);
            return;
        case ImGuiMouseCursor_ResizeNS:
            application.setCursor(Application::Cursor::ResizeNS);
            return;
        case ImGuiMouseCursor_ResizeEW:
            application.setCursor(Application::Cursor::ResizeWE);
            return;
        case ImGuiMouseCursor_ResizeNESW:
            application.setCursor(Implementation::OptionalResizeNESWCursor<Application>::Cursor);
            return;
        case ImGuiMouseCursor_ResizeNWSE:
            application.setCursor(Implementation::OptionalResizeNWSECursor<Application>::Cursor);
            return;
        case ImGuiMouseCursor_Hand:
            application.setCursor(Application::Cursor::Hand);
            return;

        /* For unknown cursors we set Arrow as well */
        case ImGuiMouseCursor_Arrow:
        default:
            application.setCursor(Application::Cursor::Arrow);
            return;
    }

    CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}}

#endif
