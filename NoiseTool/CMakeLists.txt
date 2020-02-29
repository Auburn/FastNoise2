hunter_add_package(magnum)
find_package(Magnum CONFIG REQUIRED
    GL
    MeshTools
    Primitives
    Shaders
    Sdl2Application)

add_executable(NoiseTool 
    "main.cpp"
    "NoiseToolApp.cpp")
 
target_include_directories(FastNoise PUBLIC include)

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::Shaders)
 
add_dependencies(NoiseTool FastNoise)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)