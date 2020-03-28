set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules/" ${CMAKE_MODULE_PATH})

cmake_policy(SET CMP0074 NEW)

hunter_add_package(SDL2)
find_package(SDL2 CONFIG REQUIRED)

set(HUNTER_ENABLED OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_PLUGINS_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TESTSUITE OFF CACHE BOOL "" FORCE)

# Add Corrade as a subproject
add_subdirectory(ThirdParty/corrade EXCLUDE_FROM_ALL)

# Add Magnum as a subproject, enable Sdl2Application
set(WITH_SDL2APPLICATION ON CACHE BOOL "" FORCE)

add_subdirectory(ThirdParty/magnum EXCLUDE_FROM_ALL)
    
find_package(Magnum REQUIRED GL Sdl2Application)
    
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui/")
set(WITH_IMGUI ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/magnum-integration EXCLUDE_FROM_ALL)

find_package(MagnumIntegration REQUIRED ImGui)
    
set(HUNTER_ENABLED ON CACHE BOOL "" FORCE)

add_executable(NoiseTool 
    "NoiseToolApp.cpp"
    "FastNoiseNodeEditor.cpp"
    "imnodes.cpp")
 
target_link_libraries(NoiseTool PUBLIC
    FastNoise
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    MagnumIntegration::ImGui
)
 
add_dependencies(NoiseTool FastNoise)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)
