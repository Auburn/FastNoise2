#define FASTNOISE_EXPORT
#include "FastNoise/NodeEditorIpc_C.h"

#include <atomic>
#include <cstring>
#include <string>

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
#include <dlfcn.h>      // For dladdr
#include <libgen.h>     // For dirname
#endif
#endif

// For filesystem operations
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

static constexpr const char* kSharedMemoryName = "/FastNoise2NodeEditor";
static constexpr unsigned int kSharedMemorySize = 64 * 1024;

#ifdef _WIN32
static HMODULE g_hModule = nullptr;

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
    if( fdwReason == DLL_PROCESS_ATTACH )
    {
        g_hModule = hinstDLL;
    }
    return TRUE;
}
#endif

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
    if( !ipc )
    {
        return FASTNOISE_EDITORIPC_MSG_IPC_NOT_INITIALISED;
    }

    IpcContext* ctx = static_cast<IpcContext*>( ipc );

    if( !ctx->sharedMemory )
    {
        return FASTNOISE_EDITORIPC_MSG_IPC_NOT_INITIALISED;
    }

    const unsigned char* sharedMemory = static_cast<const unsigned char*>( ctx->sharedMemory );
    const unsigned char sharedCounter = sharedMemory[0];
    const unsigned char dataType = sharedMemory[1];

    // Check if counter changed (new message)
    if( sharedCounter == ctx->lastCounter )
    {
        return FASTNOISE_EDITORIPC_MSG_NONE;
    }

    // Validate message type
    if( dataType != FASTNOISE_EDITORIPC_MSG_SELECTED_NODE && dataType != FASTNOISE_EDITORIPC_MSG_IMPORT_REQUEST )
    {
        ctx->lastCounter = sharedCounter;
        return FASTNOISE_EDITORIPC_MSG_NONE;
    }

    if( !outBuffer || bufferSize <= 0 )
    {
        return FASTNOISE_EDITORIPC_MSG_BUFFER_TOO_SMALL;
    }
       
    ctx->lastCounter = sharedCounter;

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

// Global override for NodeEditor binary path
static std::string g_nodeEditorPath;

void fnEditorIpcSetNodeEditorPath( const char* path )
{
    if( path )
    {
        g_nodeEditorPath = path;
    }
    else
    {
        g_nodeEditorPath.clear();
    }
}

// Helper to find the NodeEditor executable
static std::string FindNodeEditorBinary()
{
    // If path was manually set, use it
    if( !g_nodeEditorPath.empty() )
    {
        return g_nodeEditorPath;
    }

#ifdef _WIN32
    char modulePath[MAX_PATH];
    
    // Try to get path
    if( GetModuleFileNameA( g_hModule, modulePath, MAX_PATH ) )
    {
        // Extract directory from DLL path
        std::string moduleDir( modulePath );
        size_t lastSlash = moduleDir.find_last_of( "\\/" );
        if( lastSlash != std::string::npos )
        {
            moduleDir = moduleDir.substr( 0, lastSlash );
            
            // Try: same directory as DLL
            std::string candidate = moduleDir + "\\NodeEditor.exe";
            if( GetFileAttributesA( candidate.c_str() ) != INVALID_FILE_ATTRIBUTES )
            {
                return candidate;
            }
            
            // Try: ../bin relative to DLL
            candidate = moduleDir + "\\..\\bin\\NodeEditor.exe";
            if( GetFileAttributesA( candidate.c_str() ) != INVALID_FILE_ATTRIBUTES )
            {
                return candidate;
            }
        }
    }
    
    return "";
#else
    Dl_info dlInfo;
    
    // Try to get SO path using a function pointer from this library (works for dynamic library)
    if( dladdr( (void*)fnEditorIpcSetup, &dlInfo ) && dlInfo.dli_fname )
    {
        char* pathCopy = strdup( dlInfo.dli_fname );
        char* soDir = dirname( pathCopy );
        
        // Try: same directory as SO
        std::string candidate = std::string( soDir ) + "/NodeEditor";
        if( access( candidate.c_str(), X_OK ) == 0 )
        {
            free( pathCopy );
            return candidate;
        }
        
        // Try: ../bin relative to SO
        candidate = std::string( soDir ) + "/../bin/NodeEditor";
        if( access( candidate.c_str(), X_OK ) == 0 )
        {
            free( pathCopy );
            return candidate;
        }
        
        free( pathCopy );
    }
    
    // Static library fallback: try common paths relative to current working directory
    // Try: ./NodeEditor
    if( access( "./NodeEditor", X_OK ) == 0 )
    {
        return "./NodeEditor";
    }
    
    // Try: ../bin/NodeEditor
    if( access( "../bin/NodeEditor", X_OK ) == 0 )
    {
        return "../bin/NodeEditor";
    }
    
    // Try: ./bin/NodeEditor
    if( access( "./bin/NodeEditor", X_OK ) == 0 )
    {
        return "./bin/NodeEditor";
    }
    
    return "";
#endif
}

bool fnEditorIpcStartNodeEditor( const char* encodedNodeTree, bool detached, bool childProcess )
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    // Find the NodeEditor binary
    std::string editorPath = FindNodeEditorBinary();
    if( editorPath.empty() )
    {
        return false;
    }
    
    // Build command line arguments
    std::string cmdLine = "\"" + editorPath + "\"";
    
    if( detached )
    {
        cmdLine += " --detached";
    }
    
    if( encodedNodeTree && encodedNodeTree[0] != '\0' )
    {
        cmdLine += " --import-ent ";
        cmdLine += encodedNodeTree;
    }
    
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );
    
    HANDLE hJob = nullptr;
    
    if( childProcess )
    {
        // Create a job object to terminate child when parent exits
        hJob = CreateJobObjectA( NULL, NULL );
        if( hJob )
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
            ZeroMemory( &jeli, sizeof( jeli ) );
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject( hJob, JobObjectExtendedLimitInformation, &jeli, sizeof( jeli ) );
        }
    }
    
    // Start the child process
    if( !CreateProcessA( NULL,                  // No module name (use command line)
                        (LPSTR)cmdLine.c_str(), // Command line
                        NULL,                   // Process handle not inheritable
                        NULL,                   // Thread handle not inheritable
                        FALSE,                  // Set handle inheritance to FALSE
                        0,                      // No creation flags
                        NULL,                   // Use parent's environment block
                        NULL,                   // Use parent's starting directory
                        &si,                    // Pointer to STARTUPINFO structure
                        &pi ) )                 // Pointer to PROCESS_INFORMATION structure
    {
        if( hJob )
        {
            CloseHandle( hJob );
        }
        return false;
    }
    
    // Assign the child process to the job object
    if( hJob )
    {
        AssignProcessToJobObject( hJob, pi.hProcess );
    }
    
    // Close handles to the child process and primary thread
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    
    return true;
    
#else
    pid_t pid = fork(); // Duplicate current process
    
    if( pid == 0 )
    {
        // Child process
        if( !childProcess )
        {
            // Create a new session so the child survives parent exit
            setsid();
        }
        
        // Build argv array
        const char* executable = editorPath.c_str();
        
        if( detached && encodedNodeTree && encodedNodeTree[0] != '\0' )
        {
            execl( executable, executable, "--detached", "--import-ent", encodedNodeTree, (char*)NULL );
        }
        else if( detached )
        {
            execl( executable, executable, "--detached", (char*)NULL );
        }
        else if( encodedNodeTree && encodedNodeTree[0] != '\0' )
        {
            execl( executable, executable, "--import-ent", encodedNodeTree, (char*)NULL );
        }
        else
        {
            execl( executable, executable, (char*)NULL );
        }
        
        // If execl returns, it means it has failed
        exit( EXIT_FAILURE );
    }
    
    if( pid < 0 )
    {
        return false;
    }
    
    return true;
#endif
#endif // __EMSCRIPTEN__
}
