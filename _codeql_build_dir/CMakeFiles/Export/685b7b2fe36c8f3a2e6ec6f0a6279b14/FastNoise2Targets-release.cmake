#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "FastNoise2::FastNoise" for configuration "Release"
set_property(TARGET FastNoise2::FastNoise APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(FastNoise2::FastNoise PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libFastNoise.a"
  )

list(APPEND _cmake_import_check_targets FastNoise2::FastNoise )
list(APPEND _cmake_import_check_files_for_FastNoise2::FastNoise "${_IMPORT_PREFIX}/lib/libFastNoise.a" )

# Import target "FastNoise2::NodeEditor" for configuration "Release"
set_property(TARGET FastNoise2::NodeEditor APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(FastNoise2::NodeEditor PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/NodeEditor"
  )

list(APPEND _cmake_import_check_targets FastNoise2::NodeEditor )
list(APPEND _cmake_import_check_files_for_FastNoise2::NodeEditor "${_IMPORT_PREFIX}/bin/NodeEditor" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
