/**
 * Buffer Pool - VBO/IBO/UBO pooling and management
 * Reduces allocation overhead and fragmentation
 */

#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <GLES3/gl32.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define BUFFER_POOL_DEFAULT_SIZE    (16 * 1024 * 1024)  // 16 MB
#define BUFFER_POOL_BLOCK_SIZE      (64 * 1024)         // 64 KB blocks
#define MAX_BUFFER_POOLS            8
#define BUFFER_ALIGNMENT            256                  // GPU alignment

// ============================================================================
// Types
// ============================================================================

/**
 * Buffer usage type
 */
typedef enum BufferUsage {
    BUFFER_USAGE_STATIC = GL_STATIC_DRAW,
    BUFFER_USAGE_DYNAMIC = GL_DYNAMIC_DRAW,
    BUFFER_USAGE_STREAM = GL_STREAM_DRAW
} BufferUsage;

/**
 * Buffer target type
 */
typedef enum BufferTarget {
    BUFFER_TARGET_VERTEX = GL_ARRAY_BUFFER,
    BUFFER_TARGET_INDEX = GL_ELEMENT_ARRAY_BUFFER,
    BUFFER_TARGET_UNIFORM = GL_UNIFORM_BUFFER,
    BUFFER_TARGET_SHADER_STORAGE = GL_SHADER_STORAGE_BUFFER,
    BUFFER_TARGET_COPY_READ = GL_COPY_READ_BUFFER,
    BUFFER_TARGET_COPY_WRITE = GL_COPY_WRITE_BUFFER
} BufferTarget;

/**
 * Buffer allocation handle
 */
typedef struct BufferAllocation {
    GLuint bufferId;           // GL buffer name
    size_t offset;             // Offset within buffer
    size_t size;               // Allocated size
    size_t alignedSize;        // Size with alignment padding
    void* mappedPtr;           // Mapped pointer (if persistent)
    uint32_t poolIndex;        // Which pool this came from
    uint32_t blockIndex;       // Block within pool
    bool persistent;           // Is persistently mapped
    bool coherent;             // Is coherent mapping
} BufferAllocation;

/**
 * Buffer pool block
 */
typedef struct BufferBlock {
    size_t offset;
    size_t size;
    bool free;
    struct BufferBlock* next;
    struct BufferBlock* prev;
} BufferBlock;

/**
 * Buffer pool
 */
typedef struct BufferPool {
    GLuint bufferId;
    BufferTarget target;
    BufferUsage usage;
    size_t totalSize;
    size_t usedSize;
    size_t freeSize;
    
    // Block management
    BufferBlock* blocks;
    int blockCount;
    
    // Persistent mapping
    void* mappedPtr;
    bool persistentMapped;
    GLsync fence;
    
    // Statistics
    uint32_t allocCount;
    uint32_t freeCount;
    uint32_t fragmentCount;
} BufferPool;

/**
 * Buffer manager context
 */
typedef struct BufferManagerContext {
    BufferPool pools[MAX_BUFFER_POOLS];
    int poolCount;
    
    // Per-frame ring buffer for streaming data
    GLuint streamBuffer;
    size_t streamBufferSize;
    size_t streamOffset;
    void* streamMappedPtr;
    GLsync streamFences[3];  // Triple buffering
    int currentFrame;
    
    // Statistics
    size_t totalAllocated;
    size_t totalUsed;
    uint32_t totalAllocations;
    
    bool initialized;
    bool persistentMappingSupported;
} BufferManagerContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize buffer manager
 */
bool bufferManagerInit(size_t poolSize);

/**
 * Shutdown buffer manager
 */
void bufferManagerShutdown(void);

/**
 * Create a buffer pool
 */
int bufferPoolCreate(BufferTarget target, BufferUsage usage, size_t size);

/**
 * Destroy a buffer pool
 */
void bufferPoolDestroy(int poolIndex);

/**
 * Allocate from pool
 */
BufferAllocation* bufferPoolAlloc(int poolIndex, size_t size);

/**
 * Free allocation back to pool
 */
void bufferPoolFree(BufferAllocation* alloc);

/**
 * Upload data to allocation
 */
void bufferUpload(BufferAllocation* alloc, const void* data, size_t size, size_t offset);

/**
 * Map buffer for writing
 */
void* bufferMap(BufferAllocation* alloc, size_t offset, size_t size);

/**
 * Unmap buffer
 */
void bufferUnmap(BufferAllocation* alloc);

/**
 * Flush mapped range
 */
void bufferFlush(BufferAllocation* alloc, size_t offset, size_t size);

// ============================================================================
// Streaming Buffer (per-frame dynamic data)
// ============================================================================

/**
 * Begin frame (rotate ring buffer)
 */
void bufferStreamBeginFrame(void);

/**
 * End frame
 */
void bufferStreamEndFrame(void);

/**
 * Allocate from stream buffer
 * Returns offset within buffer, data is written immediately
 */
size_t bufferStreamAlloc(size_t size, const void* data, GLuint* outBuffer);

/**
 * Get current stream buffer
 */
GLuint bufferStreamGetBuffer(void);

// ============================================================================
// Direct Buffer Operations
// ============================================================================

/**
 * Create a standalone buffer
 */
GLuint bufferCreate(BufferTarget target, size_t size, const void* data, BufferUsage usage);

/**
 * Delete buffer
 */
void bufferDelete(GLuint buffer);

/**
 * Bind buffer
 */
void bufferBind(BufferTarget target, GLuint buffer);

/**
 * Bind buffer range (for UBO/SSBO)
 */
void bufferBindRange(BufferTarget target, GLuint index, GLuint buffer, 
                     size_t offset, size_t size);

/**
 * Copy buffer data
 */
void bufferCopy(GLuint srcBuffer, GLuint dstBuffer, 
                size_t srcOffset, size_t dstOffset, size_t size);

// ============================================================================
// Statistics
// ============================================================================

/**
 * Get memory statistics
 */
void bufferManagerGetStats(size_t* totalAllocated, size_t* totalUsed, 
                           uint32_t* allocCount);

/**
 * Defragment pools (call during loading screens)
 */
void bufferPoolDefragment(int poolIndex);

/**
 * Trim unused memory
 */
void bufferManagerTrim(void);

#ifdef __cplusplus
}
#endif

#endif // BUFFER_POOL_H
