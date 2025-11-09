include("/home/runner/work/FastNoise2/FastNoise2/cmake/CPM.cmake")
CPMAddPackage("NAME;robinhoodhashing;GITHUB_REPOSITORY;martinus/robin-hood-hashing;GIT_TAG;3.11.5;EXCLUDE_FROM_ALL;YES")
set(robinhoodhashing_FOUND TRUE)