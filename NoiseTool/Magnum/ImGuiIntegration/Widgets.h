#ifndef Magnum_ImGuiIntegration_Widgets_h
#define Magnum_ImGuiIntegration_Widgets_h
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
 * @brief Function @ref Magnum::ImGuiIntegration::image(), @ref Magnum::ImGuiIntegration::imageButton()
 */

#include "visibility.h" /* defines IMGUI_API */

#include <imgui.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/GL.h>

#include "Integration.h"

namespace Magnum { namespace ImGuiIntegration {

/**
@brief Image widget displaying a @ref GL::Texture2D
@param texture      Texture to display
@param size         Widget size
@param uvRange      UV range on the texture (covers the whole texture by
    default)
@param tintColor    Tint color, default @cpp 0xffffffff_rgbaf @ce
@param borderColor  Border color, default @cpp 0x00000000_rgbaf @ce
*/
inline void image(GL::Texture2D& texture, const Vector2& size,
    const Range2D& uvRange = {{}, Vector2{1.0f}},
    const Color4& tintColor = Color4{1.0f},
    const Color4& borderColor = {})
{
    ImGui::Image(static_cast<ImTextureID>(&texture), ImVec2(size), ImVec2(uvRange.topLeft()), ImVec2(uvRange.bottomRight()), ImColor(tintColor), ImColor(borderColor));
}

/**
@brief ImageButton widget displaying a @ref GL::Texture2D
@param texture          Texture to display
@param size             Widget size
@param uvRange          UV range on the texture (covers the whole texture by
    default)
@param framePadding     Frame padding, negative values use the default frame
    padding
@param backgroundColor  Background color, default @cpp 0x00000000_rgbaf @ce
@param tintColor        Tint color, default @cpp 0xffffffff_rgbaf @ce
@m_since_{integration,2019,10}
*/
inline bool imageButton(GL::Texture2D& texture, const Vector2& size,
    const Range2D& uvRange = {{}, Vector2{1.0f}}, Int framePadding = -1,
    const Color4& backgroundColor = {},
    const Color4& tintColor = Color4{1.0f})
{
    return ImGui::ImageButton(static_cast<ImTextureID>(&texture), ImVec2(size), ImVec2(uvRange.topLeft()), ImVec2(uvRange.bottomRight()), framePadding, ImColor(backgroundColor), ImColor(tintColor));
}

}}

#endif
