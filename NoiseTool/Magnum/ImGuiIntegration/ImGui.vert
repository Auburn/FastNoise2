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

#ifdef EXPLICIT_UNIFORM_LOCATION
#extension GL_ARB_explicit_uniform_location: enable
#endif
#ifdef EXPLICIT_ATTRIB_LOCATION
#extension GL_ARB_explicit_attrib_location: enable
#endif

#ifdef EXPLICIT_UNIFORM_LOCATION
layout(location = 0)
#endif
uniform mediump mat4 projectionMatrix;

#ifdef EXPLICIT_ATTRIB_LOCATION
layout(location = 0)
#endif
in mediump vec2 position;

#ifdef EXPLICIT_ATTRIB_LOCATION
layout(location = 1)
#endif
in mediump vec2 textureCoords;

#ifdef EXPLICIT_ATTRIB_LOCATION
layout(location = 2)
#endif
in mediump vec4 color;

out mediump vec2 interpolatedTextureCoords;
out mediump vec4 interpolatedColor;

void main() {
    interpolatedTextureCoords = textureCoords;
    interpolatedColor = color;
    gl_Position = projectionMatrix*vec4(position.xy, 0, 1);
}
