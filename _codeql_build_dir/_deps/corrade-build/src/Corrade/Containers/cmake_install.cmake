# Install script for directory: /home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Corrade/Containers" TYPE FILE FILES
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/AnyReference.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Array.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ArrayTuple.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ArrayView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ArrayViewStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ArrayViewStlSpan.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/BigEnumSet.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/BigEnumSet.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/BitArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/BitArrayView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/constructHelpers.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Containers.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/EnumSet.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/EnumSet.hpp"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Function.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/GrowableArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/initializeHelpers.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/iterableHelpers.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Iterable.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/LinkedList.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/MoveReference.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Optional.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/OptionalStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Pair.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/PairStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Pointer.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/PointerStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Reference.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ReferenceStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/ScopeGuard.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/sequenceHelpers.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StaticArray.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StridedArrayView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StridedArrayViewStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StridedBitArrayView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StridedDimensions.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/String.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StringIterable.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StringStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StringStlHash.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StringStlView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StringView.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/StructuredBindings.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Triple.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/TripleStl.h"
    "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src/src/Corrade/Containers/Tags.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-build/src/Corrade/Containers/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
