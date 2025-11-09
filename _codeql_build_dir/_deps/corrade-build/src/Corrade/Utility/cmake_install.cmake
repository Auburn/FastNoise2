# Install script for directory: /home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/Release/lib/libCorradeUtility.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Corrade/Utility" TYPE FILE FILES
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Algorithms.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Arguments.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/AbstractHash.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Assert.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/BitAlgorithms.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Configuration.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/ConfigurationGroup.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/ConfigurationValue.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Debug.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/DebugAssert.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/DebugStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/DebugStlStringView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Endianness.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/EndiannessBatch.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Format.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/FormatStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/FormatStlStringView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/IntrinsicsSse2.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/IntrinsicsSse3.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/IntrinsicsSsse3.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/IntrinsicsSse4.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/IntrinsicsAvx.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Json.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/JsonWriter.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Macros.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Math.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Memory.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Move.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/MurmurHash2.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Path.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Resource.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Sha1.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/String.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlForwardArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlForwardString.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlForwardTuple.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlForwardTupleSizeElement.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlForwardVector.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/StlMath.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/System.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/TypeTraits.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Unicode.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/utilities.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Utility.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/VisibilityMacros.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/visibility.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Directory.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/FileWatcher.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/Tweakable.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Utility/TweakableParser.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/Release/bin/corrade-rc")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/corrade-rc")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-build/src/Corrade/Utility/CMakeFiles/corrade-rc.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-build/src/Corrade/Utility/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
