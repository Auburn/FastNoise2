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
#define in varying
#define fragmentColor gl_FragColor
#endif

/* Uniform Buffers */
    
#ifdef EXPLICIT_UNIFORM_LOCATION
layout(location = 1)
#endif

uniform highp vec4 colorTint
    #ifndef GL_ES
    = vec4(1.0)
    #endif
    ;

/* Inputs */

in highp float interpolatedLight;

/* Outputs */

#ifdef NEW_GLSL
#ifdef EXPLICIT_ATTRIB_LOCATION
layout(location = 0)
#endif
out highp vec4 fragmentColor;
#endif

void main() 
{
    highp float light;

    if(gl_FrontFacing) 
    {
        light = interpolatedLight;
    }
    else
    { 
        light = (1.0 - interpolatedLight) * 0.08;
    }

    fragmentColor = colorTint * light;
}
