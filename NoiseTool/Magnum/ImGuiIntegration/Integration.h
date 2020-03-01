#ifndef Magnum_ImGuiIntegration_Integration_h
#define Magnum_ImGuiIntegration_Integration_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
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
@brief Conversion of Dear ImGui math types

Provides conversion for the following types:

| Magnum vector type                | Equivalent ImGui type     |
| --------------------------------- | ------------------------- |
| @ref Magnum::Vector2 "Vector2"    | @cpp ImVec2 @ce <b></b>   |
| @ref Magnum::Vector4 "Vector4", @ref Magnum::Color4 "Color4" | @cpp ImVec4 @ce, @cpp ImColor @ce |
| @ref Magnum::Vector3 "Vector3", @ref Magnum::Color3 "Color3" | @cpp ImColor @ce <b></b> |

Note that conversion of @cpp ImColor @ce to @ref Magnum::Color3 "Color3" loses
the alpha channel, while in the other direction alpha will be set to @cpp 1.0f @ce.

@attention Note that @cpp ImColor @ce is *implicitly* convertible to
    @cpp int @ce, encoding its value as a 8-bit-per-channel RGBA. This means
    operations such as @cb{.cpp} Vector2{ImColor{...}} @ce will compile, but
    produce a wrong result, having the encoded value copied to all channels.
    Enable `-Wconversion` or equivalent warning on other compilers to catch the
    unwanted int-to-float conversion.

Example usage:

@snippet ImGuiIntegration.cpp Integration

@see @ref types-thirdparty-integration
*/

#include "visibility.h" /* defines IMGUI_API */

#include <imgui.h>
#include <Magnum/Types.h>
#include <Magnum/Math/Vector.h>

/* Don't list (useless) Magnum and Math namespaces without anything else */
#ifndef DOXYGEN_GENERATING_OUTPUT
namespace Magnum { namespace Math { namespace Implementation {

/* ImVec2 */
template<> struct VectorConverter<2, Float, ImVec2> {
    static Vector<2, Float> from(const ImVec2& other) {
        return {other.x, other.y};
    }

    static ImVec2 to(const Vector<2, Float>& other) {
        return {other[0], other[1]};
    }
};

/* ImVec4 */
template<> struct VectorConverter<4, Float, ImVec4> {
    static Vector<4, Float> from(const ImVec4& other) {
        return {other.x, other.y, other.z, other.w};
    }

    static ImVec4 to(const Vector<4, Float>& other) {
        return {other[0], other[1], other[2], other[3]};
    }
};

/* ImColor */
template<> struct VectorConverter<4, Float, ImColor> {
    static Vector<4, Float> from(const ImColor& other) {
        return Vector<4, Float>(other.Value);
    }

    static ImColor to(const Vector<4, Float>& other) {
        return ImVec4(other); /* Construct from ImVec4 */
    }
};

/* ImColor would be explicitly convertible to Color3 because it has an implicit
   conversion to an int and then Color3 has an explicit single-argument
   constructor, taking a Float. That would do the wrong thing, so we provide
   an explicit conversion even though in one direction it will result in a loss
   of alpha. OTOH this also allows us to do things like ImColor(0xff3366_rgbf) */
template<> struct VectorConverter<3, Float, ImColor> {
    static Vector<3, Float> from(const ImColor& other) {
        return Vector<3, Float>(other.Value.x, other.Value.y, other.Value.z);
    }

    static ImColor to(const Vector<3, Float>& other) {
        return ImVec4(Vector<4, Float>{other[0], other[1], other[2], 1.0f});
    }
};

}}}
#endif

#endif
