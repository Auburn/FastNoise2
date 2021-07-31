CPMAddPackage(
    NAME corrade
    GITHUB_REPOSITORY mosra/corrade
    GIT_TAG a8065db3c55aec214aa4e4887c4073289b2988e2
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_INTERCONNECT OFF"
        "WITH_TESTSUITE OFF"
)

# Get SDL2 windows dev release
if(WIN32)
    CPMAddPackage(
        NAME SDL2
        VERSION 2.0.14
        URL https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip
    )

    # Set SDL2 path so magnum can find it
    list(APPEND CMAKE_PREFIX_PATH "${SDL2_SOURCE_DIR}")
endif()

CPMAddPackage(
    NAME magnum
    GITHUB_REPOSITORY mosra/magnum
    GIT_TAG 360ca834e17870e56f987bb6ecca308ab9889409
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_PLUGINS_STATIC ON"
        "WITH_SDL2APPLICATION ON"
        "WITH_MESHTOOLS OFF"
        "WITH_TRADE OFF"
        "WITH_TEXT OFF"
        "WITH_TEXTURETOOLS OFF"
)
    
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG 7bfc379a23e97b6777eb80aafb50a8e6248904cd
)

CPMAddPackage(
    NAME magnum-integration
    GITHUB_REPOSITORY mosra/magnum-integration
    GIT_TAG 789f8132842536f077548bc4f2d2c3685e9f491e
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_IMGUI ON"
        "IMGUI_DIR ${imgui_SOURCE_DIR}"
)
  
CPMAddPackage(
    NAME imnodes
    VERSION 0.4
    GITHUB_REPOSITORY Nelarius/imnodes
)

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL Sdl2Application)
find_package(MagnumIntegration REQUIRED ImGui)

add_executable(NoiseTool 
    "NoiseToolApp.cpp"
    "FastNoiseNodeEditor.cpp"
    "MeshNoisePreview.cpp"
    "NoiseTexture.cpp"
    "${imnodes_SOURCE_DIR}/imnodes.cpp"
) 

# Include imnodes in NoiseTool project since setting up separate cmake library with imgui dependancy was causing issues
target_include_directories(NoiseTool PRIVATE "${imnodes_SOURCE_DIR}")

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    MagnumIntegration::ImGui
)

if (UNIX)
    target_link_options(NoiseTool PRIVATE -pthread)
endif()

set(install_targets ${install_targets} NoiseTool PARENT_SCOPE)

# Need sdl2 dll on windows, linux/mac its expected to be installed
if(WIN32)
    find_package(SDL2)
    install(
        FILES ${SDL2_DLL_RELEASE}
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)
