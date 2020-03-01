cmake_policy(SET CMP0074 NEW)

hunter_add_package(magnum)
find_package(Magnum CONFIG REQUIRED
    GL
    MeshTools
    Primitives
    Shaders
    Sdl2Application)
    
hunter_add_package(imgui)
find_package(imgui CONFIG REQUIRED)

SET(BUILD_STATIC ON CACHE BOOL "Build magnum static libs")
add_subdirectory(Magnum/ImGuiIntegration)
    
add_executable(NoiseTool 
    "main.cpp"
    "NoiseToolApp.cpp")
 
target_include_directories(FastNoise PUBLIC include)

target_link_libraries(NoiseTool PUBLIC
    FastNoise
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders
    MagnumIntegration::ImGui)
 
add_dependencies(NoiseTool FastNoise)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)