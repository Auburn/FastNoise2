#define FASTNOISE_EXPORT
#include "FastNoise/NodeEditorIpc_C.h"
#include "include/FastNoise/NodeEditorIpc_C.h"

#include <atomic>
#include <cstring>

#ifndef __EMSCRIPTEN__
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>      // For O_* constants
#include <sys/mman.h>   // For shared memory
#include <sys/stat.h>   // For mode constants
#include <unistd.h>
#include <cerrno>
#endif
#endif

static constexpr const char* kSharedMemoryName = "/FastNoise2NodeEditor";
static constexpr unsigned int kSharedMemorySize = 64 * 1024;

struct IpcContext
{
    void* sharedMemory;
    unsigned char lastCounter;
#ifdef _WIN32
    HANDLE hMapFile;
#endif
};

void* fnEditorIpcSetup( bool readPrevious )
{
#ifdef __EMSCRIPTEN__
    return nullptr;
#else
    IpcContext* ctx = new IpcContext();
    ctx->sharedMemory = nullptr;
    ctx->lastCounter = 0;
#ifdef _WIN32
    ctx->hMapFile = nullptr;
#endif

#ifdef _WIN32
    // Create a shared memory file mapping
    ctx->hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // Use paging file - shared memory
        NULL,                   // Default security attributes
        PAGE_READWRITE,         // Read/write access
        0,                      // Maximum object size (high-order DWORD)
        kSharedMemorySize,      // Maximum object size (low-order DWORD)
        kSharedMemoryName );    // Name of mapping object

    if( ctx->hMapFile == NULL )
    {
        delete ctx;
        return nullptr;
    }

    // Map a view of the file mapping into the address space of the current process
    ctx->sharedMemory = MapViewOfFile(
        ctx->hMapFile,          // Handle to map object
        FILE_MAP_ALL_ACCESS,    // Read/write permission
        0,
        0,
        kSharedMemorySize );

    if( !ctx->sharedMemory )
    {
        CloseHandle( ctx->hMapFile );
        delete ctx;
        return nullptr;
    }

#else
    // Create the shared memory object
    int shmFd = shm_open( kSharedMemoryName, O_CREAT | O_RDWR, 0666 );
    if( shmFd == -1 )
    {
        delete ctx;
        return nullptr;
    }

    // Configure the size of the shared memory object
    if( ftruncate( shmFd, kSharedMemorySize ) == -1 )
    {
        if( errno != EINVAL ) // If the error is not just because it's already the right size
        {
            close( shmFd );
            delete ctx;
            return nullptr;
        }
    }

    // Memory map the shared memory object
    ctx->sharedMemory = mmap( 0, kSharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0 );
    close( shmFd ); // Can close fd after mmap

    if( ctx->sharedMemory == MAP_FAILED )
    {
        delete ctx;
        return nullptr;
    }
#endif

    // Initialize lastCounter based on readPrevious
    const unsigned char* sharedMemory = static_cast<const unsigned char*>( ctx->sharedMemory );
    const unsigned char currentCounter = sharedMemory[0];
    const unsigned char currentMsg = sharedMemory[1];

    if( readPrevious && currentMsg == FASTNOISE_EDITORIPC_MSG_SELECTED_NODE )
    {
        // Set to invalid value so first poll detects a "change" and returns existing data
        ctx->lastCounter = currentCounter - 1;
    }
    else
    {
        // Set to current counter so first poll waits for new data
        ctx->lastCounter = currentCounter;
    }

    return ctx;
#endif // __EMSCRIPTEN__
}

void fnEditorIpcRelease( void* ipc )
{
    if( !ipc )
    {
        return;
    }

    IpcContext* ctx = static_cast<IpcContext*>( ipc );

#ifndef __EMSCRIPTEN__
#ifdef _WIN32
    if( ctx->sharedMemory )
    {
        UnmapViewOfFile( ctx->sharedMemory );
    }
    if( ctx->hMapFile )
    {
        CloseHandle( ctx->hMapFile );
    }
#else
    if( ctx->sharedMemory && ctx->sharedMemory != MAP_FAILED )
    {
        munmap( ctx->sharedMemory, kSharedMemorySize );
    }
    shm_unlink( kSharedMemoryName );
#endif
#endif

    delete ctx;
}

static bool SendMessage( void* ipc, unsigned char messageType, const char* encodedNodeTree )
{
    if( !ipc || !encodedNodeTree )
    {
        return false;
    }

    IpcContext* ctx = static_cast<IpcContext*>( ipc );

    if( !ctx->sharedMemory )
    {
        return false;
    }

    size_t length = std::strlen( encodedNodeTree );

    // Check if data fits (need 2 bytes for header + null terminator)
    if( length + 3 >= kSharedMemorySize )
    {
        return false;
    }

    unsigned char* sharedMemory = static_cast<unsigned char*>( ctx->sharedMemory );

    // Write payload first
    std::memcpy( sharedMemory + 2, encodedNodeTree, length + 1 );

    // Write message type
    sharedMemory[1] = messageType;

    // Memory fence to ensure writes are visible before counter update
    std::atomic_thread_fence( std::memory_order_acq_rel );

    // Increment counter to signal new data
    unsigned char newCounter = sharedMemory[0] + 1;
    sharedMemory[0] = newCounter;

    // Save to lastCounter so we don't read our own message
    ctx->lastCounter = newCounter;

    return true;
}

bool fnEditorIpcSendSelectedNode( void* ipc, const char* encodedNodeTree )
{
    return SendMessage( ipc, FASTNOISE_EDITORIPC_MSG_SELECTED_NODE, encodedNodeTree );
}

bool fnEditorIpcSendImportRequest( void* ipc, const char* encodedNodeTree )
{
    return SendMessage( ipc, FASTNOISE_EDITORIPC_MSG_IMPORT_REQUEST, encodedNodeTree );
}

int fnEditorIpcPollMessage( void* ipc, char* outBuffer, int bufferSize )
{
    if( !ipc || !outBuffer || bufferSize <= 0 )
    {
        return FASTNOISE_EDITORIPC_MSG_BUFFER_TOO_SMALL;
    }

    IpcContext* ctx = static_cast<IpcContext*>( ipc );

    if( !ctx->sharedMemory )
    {
        return FASTNOISE_EDITORIPC_MSG_NONE;
    }

    const unsigned char* sharedMemory = static_cast<const unsigned char*>( ctx->sharedMemory );
    const unsigned char sharedCounter = sharedMemory[0];
    const unsigned char dataType = sharedMemory[1];

    // Check if counter changed (new message)
    if( sharedCounter == ctx->lastCounter )
    {
        return FASTNOISE_EDITORIPC_MSG_NONE;
    }

    ctx->lastCounter = sharedCounter;

    // Validate message type
    if( dataType != FASTNOISE_EDITORIPC_MSG_SELECTED_NODE && dataType != FASTNOISE_EDITORIPC_MSG_IMPORT_REQUEST )
    {
        return FASTNOISE_EDITORIPC_MSG_NONE;
    }

    // Copy data to output buffer
    const char* data = reinterpret_cast<const char*>( sharedMemory + 2 );
    size_t dataLen = std::strlen( data );

    // Check buffer size (need space for string + null terminator)
    if( static_cast<int>( dataLen + 1 ) > bufferSize )
    {
        return FASTNOISE_EDITORIPC_MSG_BUFFER_TOO_SMALL;
    }

    std::memcpy( outBuffer, data, dataLen + 1 );
    return dataType;
}
