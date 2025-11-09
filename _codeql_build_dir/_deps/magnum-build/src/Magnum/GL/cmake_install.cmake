# Install script for directory: /home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/Release/lib/libMagnumGL.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Magnum/GL" TYPE FILE FILES
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/AbstractFramebuffer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/AbstractObject.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/AbstractQuery.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/AbstractShaderProgram.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/AbstractTexture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Attribute.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Buffer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Context.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/CubeMapTexture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/DefaultFramebuffer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Extensions.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Framebuffer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/GL.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Mesh.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/MeshView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/OpenGL.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/PixelFormat.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Renderbuffer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/RenderbufferFormat.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Renderer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Sampler.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Shader.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Texture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/TextureFormat.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/TimeQuery.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/Version.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/visibility.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/PipelineStatisticsQuery.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/RectangleTexture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/BufferImage.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/PrimitiveQuery.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/TextureArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/TransformFeedback.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/DebugOutput.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/BufferTexture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/BufferTextureFormat.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/CubeMapTextureArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/ImageFormat.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/MultisampleTexture.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/GL/SampleQuery.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-build/src/Magnum/GL/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
