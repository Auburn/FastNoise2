# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-src")
  file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-build"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/tmp"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/src/magnum-integration-populate-stamp"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/src"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/src/magnum-integration-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/src/magnum-integration-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/magnum-integration-subbuild/magnum-integration-populate-prefix/src/magnum-integration-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
