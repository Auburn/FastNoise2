# Build NodeEditorIpc library (not needed for Emscripten)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_subdirectory(NodeEditorIpc)
endif()

add_subdirectory(NodeEditor)

set(install_targets ${install_targets} PARENT_SCOPE)