# NodeEditorIpc - Shared memory IPC library for FastNoise2 Node Editor

add_library(NodeEditorIpc
    "NodeEditorIpc_C.cpp"
    "include/FastNoise/NodeEditorIpc_C.h"
)

target_include_directories(NodeEditorIpc PUBLIC
    $<BUILD_INTERFACE:${FastNoise2_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(NodeEditorIpc PRIVATE FASTNOISE_STATIC_LIB)
endif()

set_target_properties(NodeEditorIpc PROPERTIES
    DEBUG_POSTFIX D
    COMPILE_PDB_NAME_DEBUG NodeEditorIpcD
)

# Link rt library on Linux (required for shm_open)
if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_libraries(NodeEditorIpc PRIVATE rt)
endif()

# Install headers
install(
    DIRECTORY "include/FastNoise"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

set(install_targets ${install_targets} NodeEditorIpc PARENT_SCOPE)
