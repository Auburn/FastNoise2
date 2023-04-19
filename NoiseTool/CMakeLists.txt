CPMAddPackage(
    NAME corrade
    GITHUB_REPOSITORY mosra/corrade
    GIT_TAG 83dba87f3c04031235da6362524e0ec475bb21e8
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "CORRADE_BUILD_STATIC ON"
        "CORRADE_BUILD_STATIC_UNIQUE_GLOBALS OFF"
        "CORRADE_MSVC_COMPATIBILITY ON"
        "CORRADE_WITH_INTERCONNECT OFF"
        "CORRADE_WITH_TESTSUITE OFF"
)

CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.8
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "GLFW_INSTALL OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_DOCS OFF"
)

CPMAddPackage(
    NAME magnum
    GITHUB_REPOSITORY mosra/magnum
    GIT_TAG c1239b66199926c610cd863e18a4d27a4cef67f1
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "MAGNUM_BUILD_STATIC ON"
        "MAGNUM_BUILD_PLUGINS_STATIC ON"
        "MAGNUM_BUILD_STATIC_UNIQUE_GLOBALS OFF"
        "MAGNUM_WITH_GLFWAPPLICATION ON"
        "MAGNUM_WITH_MESHTOOLS OFF"
        "MAGNUM_WITH_TRADE OFF"
        "MAGNUM_WITH_TEXT OFF"
        "MAGNUM_WITH_TEXTURETOOLS OFF"
)
    
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG 0ea3b87bd63ecbf359585b7c235839146e84dedb
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)
# Set dir for find_package(ImGui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

CPMAddPackage(
    NAME magnum-integration
    GITHUB_REPOSITORY mosra/magnum-integration
    GIT_TAG 1a66b05bd7db0a5484366054ddc678bebf79921e
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "MAGNUM_WITH_IMGUI ON"
)

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL GlfwApplication)
find_package(MagnumIntegration REQUIRED ImGui)
find_package(ImGui REQUIRED SourcesMiscCpp)
  
CPMAddPackage(
    NAME imnodes
    GITHUB_REPOSITORY Auburn/imnodes
    GIT_TAG 1aa48f4af2a4f9f1b9a6ed53fe858ed76646b233
    GIT_SUBMODULES ".github"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "IMNODES_IMGUI_TARGET_NAME MagnumIntegration::ImGui"
)
  
CPMAddPackage(
    NAME robinhoodhashing
    GITHUB_REPOSITORY martinus/robin-hood-hashing
    GIT_TAG 3.11.5    
    EXCLUDE_FROM_ALL YES
)

# Ensure FastNoise.dll is built into the same dir as NoiseTool.exe
set_target_properties(FastNoise
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Bundle a better font
# Configure resource file for imgui source dir variable
set(NoiseTool_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
configure_file("resources.conf" "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")
corrade_add_resource(NoiseTool_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")

add_executable(NoiseTool
    "NoiseToolApp.cpp"
    "FastNoiseNodeEditor.cpp"
    "MeshNoisePreview.cpp"
    "NoiseTexture.cpp"
    ${NoiseTool_RESOURCES}
) 

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    MagnumIntegration::ImGui
    ImGui::SourcesMiscCpp
    imnodes
    robin_hood
)

# Windows HiDPI support
if(CORRADE_TARGET_WINDOWS)
    target_sources(NoiseTool PRIVATE WindowsHiDPI.manifest)
endif()

if (UNIX)
    target_link_options(NoiseTool PRIVATE -pthread)

    if(APPLE)
        set_property(TARGET NoiseTool PROPERTY
            INSTALL_RPATH "@loader_path/../lib")
    else()
        set_property(TARGET NoiseTool PROPERTY
            INSTALL_RPATH "\$ORIGIN/../lib")
    endif()
endif()

if (MSVC)
    target_compile_definitions(NoiseTool PRIVATE _CRT_SECURE_NO_WARNINGS=1)
endif()

set(install_targets ${install_targets} NoiseTool PARENT_SCOPE)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)
