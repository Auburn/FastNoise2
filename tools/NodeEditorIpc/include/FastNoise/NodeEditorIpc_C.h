#ifndef FASTNOISE_NODEEDITORIPC_H
#define FASTNOISE_NODEEDITORIPC_H

#include "FastNoise/Utility/Export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Message types / return codes
#define FASTNOISE_EDITORIPC_MSG_BUFFER_TOO_SMALL (-1)
#define FASTNOISE_EDITORIPC_MSG_NONE              0
#define FASTNOISE_EDITORIPC_MSG_SELECTED_NODE     1
#define FASTNOISE_EDITORIPC_MSG_IMPORT_REQUEST    2

/// <summary>
/// Setup shared memory IPC for communication with the FastNoise2 Node Editor
/// </summary>
/// <param name="readPrevious">If true, first poll returns existing selected nodes; if false, only new updates</param>
/// <returns>IPC context pointer, or nullptr on failure</returns>
FASTNOISE_API void* fnEditorIpcSetup( bool readPrevious );

/// <summary>
/// Release shared memory IPC resources
/// </summary>
/// <param name="ipc">IPC context pointer from fnEditorIpcSetup</param>
FASTNOISE_API void fnEditorIpcRelease( void* ipc );

/// <summary>
/// Send currently selected node tree (typically called by editor)
/// </summary>
/// <param name="ipc">IPC context pointer</param>
/// <param name="encodedNodeTree">Base64 encoded node tree string</param>
/// <returns>true on success, false on failure</returns>
FASTNOISE_API bool fnEditorIpcSendSelectedNode( void* ipc, const char* encodedNodeTree );

/// <summary>
/// Send import request to load a node tree into the editor
/// </summary>
/// <param name="ipc">IPC context pointer</param>
/// <param name="encodedNodeTree">Base64 encoded node tree string to import</param>
/// <returns>true on success, false on failure</returns>
FASTNOISE_API bool fnEditorIpcSendImportRequest( void* ipc, const char* encodedNodeTree );

/// <summary>
/// Poll for new messages from IPC
/// </summary>
/// <param name="ipc">IPC context pointer</param>
/// <param name="outBuffer">Buffer to receive the encoded node tree string</param>
/// <param name="bufferSize">Size of outBuffer in bytes</param>
/// <returns>Message type (1 = selected node, 2 = import request), 0 if no new message, -1 if buffer too small</returns>
FASTNOISE_API int fnEditorIpcPollMessage( void* ipc, char* outBuffer, int bufferSize );

/// <summary>
/// Set the full path to the NodeEditor executable (overrides automatic discovery)
/// </summary>
/// <param name="path">Full path to NodeEditor executable, or NULL to clear</param>
FASTNOISE_API void fnEditorIpcSetNodeEditorPath( const char* path );

/// <summary>
/// Start a NodeEditor process
/// </summary>
/// <param name="encodedNodeTree">Initial encoded node tree to load, or NULL for default</param>
/// <param name="detached">If true, start in detached node graph mode (no 3D preview panel)</param>
/// <param name="childProcess">If true, the spawned editor dies when the calling process exits</param>
/// <returns>true on success, false if NodeEditor binary was not found or launch failed</returns>
FASTNOISE_API bool fnEditorIpcStartNodeEditor( const char* encodedNodeTree, bool detached, bool childProcess );

#ifdef __cplusplus
}
#endif

#endif // FASTNOISE_NODEEDITORIPC_H
