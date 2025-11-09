include("/home/runner/work/FastNoise2/FastNoise2/cmake/CPM.cmake")
CPMAddPackage("NAME;imnodes;GITHUB_REPOSITORY;Auburn/imnodes;GIT_TAG;db2ef1192a4ddff32a838094de7127142a731ef0;GIT_SUBMODULES;.github;EXCLUDE_FROM_ALL;YES;OPTIONS;BUILD_SHARED_LIBS OFF;IMNODES_IMGUI_TARGET_NAME MagnumIntegration::ImGui")
set(imnodes_FOUND TRUE)