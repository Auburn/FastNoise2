include("/home/runner/work/FastNoise2/FastNoise2/cmake/CPM.cmake")
CPMAddPackage("NAME;magnum-integration;GITHUB_REPOSITORY;mosra/magnum-integration;GIT_TAG;30d179f341eafb2b69d9c29d9b7af2b736122786;GIT_SUBMODULES;src;EXCLUDE_FROM_ALL;YES;OPTIONS;BUILD_STATIC ON;MAGNUM_WITH_IMGUI ON")
set(magnum-integration_FOUND TRUE)