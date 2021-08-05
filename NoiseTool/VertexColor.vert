#if (!defined(GL_ES) && __VERSION__ >= 130) || (defined(GL_ES) && __VERSION__ >= 300)
    #define NEW_GLSL
#endif

#if !defined(GL_ES) && defined(GL_ARB_explicit_attrib_location) && !defined(DISABLE_GL_ARB_explicit_attrib_location)
    #extension GL_ARB_explicit_attrib_location: enable
    #define EXPLICIT_ATTRIB_LOCATION
#endif

#if !defined(GL_ES) && defined(GL_ARB_shading_language_420pack) && !defined(DISABLE_GL_ARB_shading_language_420pack)
    #extension GL_ARB_shading_language_420pack: enable
    #define RUNTIME_CONST
    #define EXPLICIT_TEXTURE_LAYER
#endif

#if !defined(GL_ES) && defined(GL_ARB_explicit_uniform_location) && !defined(DISABLE_GL_ARB_explicit_uniform_location)
    #extension GL_ARB_explicit_uniform_location: enable
    #define EXPLICIT_UNIFORM_LOCATION
#endif

#if defined(GL_ES) && __VERSION__ >= 300
    #define EXPLICIT_ATTRIB_LOCATION
    /* EXPLICIT_TEXTURE_LAYER, EXPLICIT_UNIFORM_LOCATION and RUNTIME_CONST is not
       available in OpenGL ES */
#endif

/* Precision qualifiers are not supported in GLSL 1.20 */
#if !defined(GL_ES) && __VERSION__ == 120
    #define highp
    #define mediump
    #define lowp
#endif

#ifndef NEW_GLSL
#define in attribute
#define out varying
#endif

/* Uniform buffers */

#ifdef EXPLICIT_UNIFORM_LOCATION
layout(location = 0)
#endif

uniform highp mat4 transformationProjectionMatrix
    #ifndef GL_ES
    = mat4(1.0)
    #endif
    ;

/* Inputs */

#ifdef EXPLICIT_ATTRIB_LOCATION
layout(location = 0)
#endif
in highp vec4 positionLight;

/* Outputs */

out highp float interpolatedLight;

void main() 
{
    gl_Position = transformationProjectionMatrix * vec4(positionLight.xyz, 1.0);

    interpolatedLight = positionLight.w;
}
