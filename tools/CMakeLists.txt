CPMAddPackage(
    NAME corrade
    GITHUB_REPOSITORY mosra/corrade
    GIT_TAG 295bbba1f49887da060465f88b8501965f6acd7d
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "CORRADE_BUILD_STATIC ON"
        "CORRADE_BUILD_STATIC_UNIQUE_GLOBALS OFF"
        "CORRADE_MSVC_COMPATIBILITY ON"
        "CORRADE_WITH_INTERCONNECT OFF"
        "CORRADE_WITH_TESTSUITE OFF"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(NODE_EDITOR_APP_TYPE_CAPS "EMSCRIPTEN")   
    set(NODE_EDITOR_APP_TYPE "Emscripten")   
else()
    set(NODE_EDITOR_APP_TYPE_CAPS "GLFW")   
    set(NODE_EDITOR_APP_TYPE "Glfw")   

    CPMAddPackage(
        NAME GLFW
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.3.9
        EXCLUDE_FROM_ALL YES
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "GLFW_INSTALL OFF"
            "GLFW_BUILD_TESTS OFF"
            "GLFW_BUILD_EXAMPLES OFF"
            "GLFW_BUILD_DOCS OFF"
    )
endif()

CPMAddPackage(
    NAME magnum
    GITHUB_REPOSITORY mosra/magnum
    GIT_TAG c9a884938c606b7d4555da6d278d1f3e09588c3e
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "MAGNUM_BUILD_STATIC ON"
        "MAGNUM_BUILD_PLUGINS_STATIC ON"
        "MAGNUM_BUILD_STATIC_UNIQUE_GLOBALS OFF"
        "MAGNUM_WITH_${NODE_EDITOR_APP_TYPE_CAPS}APPLICATION ON"
        "MAGNUM_WITH_MESHTOOLS OFF"
        "MAGNUM_WITH_TRADE OFF"
        "MAGNUM_WITH_TEXT OFF"
        "MAGNUM_WITH_TEXTURETOOLS OFF"
        "MAGNUM_TARGET_GLES2 OFF"
)
    
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.90.1-docking
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)
# Set dir for find_package(ImGui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

CPMAddPackage(
    NAME magnum-integration
    GITHUB_REPOSITORY mosra/magnum-integration
    GIT_TAG f01593fc94556bff23a848ac71187c56e034b6d9
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "MAGNUM_WITH_IMGUI ON"
)

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL ${NODE_EDITOR_APP_TYPE}Application)
find_package(MagnumIntegration REQUIRED ImGui)
find_package(ImGui REQUIRED SourcesMiscCpp)
  
CPMAddPackage(
    NAME imnodes
    GITHUB_REPOSITORY Auburn/imnodes
    GIT_TAG 26b70c528d48beeb839035f3da71550f8b0adfa7
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

# Ensure FastNoise.dll is built into the same dir as NodeEditor.exe
set_target_properties(FastNoise
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Bundle a better font
# Configure resource file for imgui source dir variable
set(NodeEditor_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
configure_file("resources.conf" "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")
corrade_add_resource(NodeEditor_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")

add_executable(NodeEditor
    
    "NodeEditorApp.cpp"
    "FastNoiseNodeEditor.cpp"
    "MeshNoisePreview.cpp"
    "NoiseTexture.cpp"
    ${NodeEditor_RESOURCES}
) 

target_link_libraries(NodeEditor PRIVATE
    FastNoise
    #FastSIMD_FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    MagnumIntegration::ImGui
    ImGui::SourcesMiscCpp
    imnodes
    robin_hood
)

target_compile_features(NodeEditor PRIVATE cxx_std_20)

# Windows HiDPI support
if(CORRADE_TARGET_WINDOWS)
    target_sources(NodeEditor PRIVATE WindowsHiDPI.manifest)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_options(NodeEditor PRIVATE -pthread -msimd128)
    target_link_options(NodeEditor PRIVATE 
        "-sPTHREAD_POOL_SIZE=Math.max(2,navigator.hardwareConcurrency)+3-navigator.hardwareConcurrency/4"
        -pthread -sALLOW_MEMORY_GROWTH=1 -lidbfs.js -s FORCE_FILESYSTEM
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/emscripten_shell.html" 
        --pre-js "${CMAKE_CURRENT_SOURCE_DIR}/emscripten_pre.js"
        -Wl,-u,_emscripten_run_callback_on_thread
    )
    add_custom_command(TARGET NodeEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/emscripten_enable_shared_array_buffer.js"
        $<TARGET_FILE_DIR:NodeEditor>
    )

elseif (UNIX)
    target_link_options(NodeEditor PRIVATE -pthread)

    if(APPLE)
        set_property(TARGET NodeEditor PROPERTY
            INSTALL_RPATH "@loader_path/../lib")
    else()
        set_property(TARGET NodeEditor PROPERTY
            INSTALL_RPATH "\$ORIGIN/../lib")
    endif()
endif()

if (MSVC)
    target_compile_definitions(NodeEditor PRIVATE _CRT_SECURE_NO_WARNINGS=1)
endif()

set(install_targets ${install_targets} NodeEditor PARENT_SCOPE)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NodeEditor)
