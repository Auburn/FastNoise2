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

CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.4
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
    GIT_TAG 360ca834e17870e56f987bb6ecca308ab9889409
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_PLUGINS_STATIC ON"
        "WITH_GLFWAPPLICATION ON"
        "WITH_MESHTOOLS OFF"
        "WITH_TRADE OFF"
        "WITH_TEXT OFF"
        "WITH_TEXTURETOOLS OFF"
)
    
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG 7bfc379a23e97b6777eb80aafb50a8e6248904cd
    EXCLUDE_FROM_ALL YES
)
# Set dir for find_package(ImGui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

CPMAddPackage(
    NAME magnum-integration
    GITHUB_REPOSITORY mosra/magnum-integration
    GIT_TAG 789f8132842536f077548bc4f2d2c3685e9f491e
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_IMGUI ON"
)
  
CPMAddPackage(
    NAME imnodes
    GITHUB_REPOSITORY Auburn/imnodes
    GIT_TAG a295b4af69045645d657363d5d135e2d8a3785f6
    EXCLUDE_FROM_ALL YES
)
  
CPMAddPackage(
    NAME robinhoodhashing
    GITHUB_REPOSITORY martinus/robin-hood-hashing
    GIT_TAG 3.11.3    
    EXCLUDE_FROM_ALL YES
)

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL GlfwApplication)
find_package(MagnumIntegration REQUIRED ImGui)
find_package(ImGui REQUIRED SourcesMiscCpp)

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
    "${imnodes_SOURCE_DIR}/imnodes.cpp"
    ${NoiseTool_RESOURCES}
) 

# Include imnodes in NoiseTool project since setting up separate cmake library with imgui dependancy was causing issues
target_include_directories(NoiseTool PRIVATE 
    "${imnodes_SOURCE_DIR}")

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    MagnumIntegration::ImGui
    ImGui::SourcesMiscCpp
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
