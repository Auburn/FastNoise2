# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src")
  file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-build"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/tmp"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/src/corrade-populate-stamp"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/src"
  "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/src/corrade-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/src/corrade-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/FastNoise2/FastNoise2/_codeql_build_dir/_deps/corrade-subbuild/corrade-populate-prefix/src/corrade-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
