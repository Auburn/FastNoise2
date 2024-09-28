#ifndef __EMSCRIPTEN__
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h> // For O_* constants
#include <sys/mman.h> // For shared memory
#include <sys/stat.h> // For mode constants
#include <unistd.h>
#endif
#endif

static constexpr const char* kSharedMemoryName = "/FastNoise2NodeEditor";
static constexpr unsigned int kSharedMemorySize = 64 * 1024;

// Setup shared memory for IPC selected node ENT updates
void* FastNoiseNodeEditor::SetupSharedMemoryIpc()
{
#ifdef __EMSCRIPTEN__
    return nullptr;
#elif defined( WIN32 )
    // Create a shared memory file mapping
    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE, // Use paging file - shared memory
        NULL, // Default security attributes
        PAGE_READWRITE, // Read/write access
        0, // Maximum object size (high-order DWORD)
        kSharedMemorySize, // Maximum object size (low-order DWORD)
        kSharedMemoryName ); // Name of mapping object

    if( hMapFile == NULL )
    {
        Debug {} << "Failed to create IPC shared memory object" << GetLastError();
        return nullptr;
    }

    // Map a view of the file mapping into the address space of the current process
    void* ptr = MapViewOfFile( hMapFile, // Handle to map object
        FILE_MAP_ALL_ACCESS, // Read/write permission
        0,
        0,
        kSharedMemorySize );

    if( !ptr )
    {
        Debug {} << "Failed to map IPC shared memory" << GetLastError();
    }
    return ptr;

#else
    // Create the shared memory object
    int shmFd = shm_open( kSharedMemoryName, O_CREAT | O_RDWR, 0666 );
    if( shmFd == -1 )
    {
        Debug {} << "Failed to create IPC shared memory object";
        return nullptr;
    }

    // Configure the size of the shared memory object
    if( ftruncate( shmFd, kSharedMemorySize ) == -1 )
    {
        if( errno != EINVAL ) // If the error is not just because it's already the right size
        {
            Debug {} << "Failed to config IPC shared memory object";
            return nullptr;
        }
    }

    // Memory map the shared memory object
    void* ptr = mmap( 0, kSharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0 );
    if( ptr == MAP_FAILED )
    {
        Debug {} << "Failed to map IPC shared memory object";
        return nullptr;
    }
    return ptr;
#endif
}

void FastNoiseNodeEditor::ReleaseSharedMemoryIpc()
{
#if !defined( WIN32 ) && !defined( __EMSCRIPTEN__ )
    shm_unlink( kSharedMemoryName );
#endif
}

// Poll for changes in the shared memory space
void FastNoiseNodeEditor::DoIpcPolling()
{
    const void* sharedMemory = mNodeEditorApp.GetIpcSharedMemory();

    if( sharedMemory )
    {
        const unsigned char sharedCounter = *static_cast<const unsigned char*>( sharedMemory );
        const unsigned char dataType = *( static_cast<const unsigned char*>( sharedMemory ) + 1 );

        // Invalidate the counter to read initial stale data only if it's type 0
        static int counter = ( dataType == 0 ) ? 0xFFFFFF : sharedCounter;

        if( sharedCounter != counter )
        {
            counter = sharedCounter;

            // Check type
            switch( dataType )
            {
            default:
                Debug {} << "Unknown IPC data type" << dataType;
                break;
            case 0: // Selected node ENT
            {
                std::string newEncodedNodeTree = static_cast<const char*>( sharedMemory ) + 2;

                SetPreviewGenerator( newEncodedNodeTree );
            }
            break;
            }
        }
    }
}