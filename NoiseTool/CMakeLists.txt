set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_PLUGINS_STATIC ON CACHE BOOL "" FORCE)

set(WITH_TESTSUITE OFF CACHE BOOL "" FORCE)

# Add Corrade as a subproject
add_subdirectory(ThirdParty/corrade EXCLUDE_FROM_ALL)

# Set SDL2 path
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL2-2.0.12")

# Add Magnum as a subproject, enable Sdl2Application
set(WITH_SDL2APPLICATION ON CACHE BOOL "" FORCE)
set(WITH_MESHTOOLS OFF CACHE BOOL "" FORCE)
set(WITH_TEXT OFF CACHE BOOL "" FORCE)
set(WITH_TEXTURETOOLS OFF CACHE BOOL "" FORCE)
set(WITH_TRADE OFF CACHE BOOL "" FORCE)

add_subdirectory(ThirdParty/magnum EXCLUDE_FROM_ALL)
    
find_package(Magnum REQUIRED GL Sdl2Application)
    
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui")
set(WITH_IMGUI ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/magnum-integration EXCLUDE_FROM_ALL)

find_package(MagnumIntegration REQUIRED ImGui)
    
include_directories(ThirdParty/imnodes)

add_executable(NoiseTool 
    "NoiseToolApp.cpp"
    "FastNoiseNodeEditor.cpp"
    "MeshNoisePreview.cpp"
    "NoiseTexture.cpp"
    "ThirdParty/imnodes/imnodes.cpp")
 
set(install_targets ${install_targets} NoiseTool PARENT_SCOPE)

target_link_libraries(NoiseTool PUBLIC
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    MagnumIntegration::ImGui
)

if (UNIX)
    target_link_options(NoiseTool PRIVATE -pthread)
endif()

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)
