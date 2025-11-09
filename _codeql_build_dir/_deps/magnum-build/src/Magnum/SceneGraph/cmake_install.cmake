# Install script for directory: /home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/Release/lib/libMagnumSceneGraph.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Magnum/SceneGraph" TYPE FILE FILES
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractFeature.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractFeature.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractGroupedFeature.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractObject.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTransformation.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTranslation.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTranslationRotation2D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTranslationRotation3D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTranslationRotationScaling2D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AbstractTranslationRotationScaling3D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Animable.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Animable.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/AnimableGroup.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Camera.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Camera.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Drawable.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Drawable.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/DualComplexTransformation.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/DualQuaternionTransformation.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/RigidMatrixTransformation2D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/RigidMatrixTransformation2D.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/RigidMatrixTransformation3D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/RigidMatrixTransformation3D.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/FeatureGroup.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/FeatureGroup.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/MatrixTransformation2D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/MatrixTransformation2D.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/MatrixTransformation3D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/MatrixTransformation3D.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Object.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Object.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/Scene.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/SceneGraph.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/TranslationTransformation.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/TranslationRotationScalingTransformation2D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-src/src/Magnum/SceneGraph/visibility.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-build/src/Magnum/SceneGraph/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
