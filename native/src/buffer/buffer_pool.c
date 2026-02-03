/**
 * Buffer Pool - Implementation
 */

#include "buffer_pool.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../core/gl_wrapper.h"

#include <string.h>
#include <pthread.h>

// ============================================================================
// Forward declarations
// ============================================================================

bool glExtensionSupported(const char* extension);

// ============================================================================
// GL Extension constants (not in standard GLES headers)
// ============================================================================

#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif

#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif

// Function pointer for glBufferStorage
typedef void (*PFNGLBUFFERSTORAGEPROC)(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);
static PFNGLBUFFERSTORAGEPROC glBufferStorageEXT = NULL;

// ============================================================================
// Global State
// ============================================================================

static BufferManagerContext* g_bufMgr = NULL;
static pthread_mutex_t g_bufMutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// Helper Functions
// ============================================================================

static size_t alignSize(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static bool checkPersistentMappingSupport(void) {
    bool hasExtension = glExtensionSupported("GL_EXT_buffer_storage");
    
    if (hasExtension && !glBufferStorageEXT) {
        glBufferStorageEXT = (PFNGLBUFFERSTORAGEPROC)eglGetProcAddress("glBufferStorageEXT");
    }
    
    return hasExtension && glBufferStorageEXT != NULL;
}

// ============================================================================
// Initialization
// ============================================================================

bool bufferManagerInit(size_t poolSize) {
    pthread_mutex_lock(&g_bufMutex);
    
    if (g_bufMgr) {
        velocityLogWarn("Buffer manager already initialized");
        pthread_mutex_unlock(&g_bufMutex);
        return true;
    }
    
    velocityLogInfo("Initializing buffer manager");
    
    g_bufMgr = (BufferManagerContext*)velocityCalloc(1, sizeof(BufferManagerContext));
    if (!g_bufMgr) {
        velocityLogError("Failed to allocate buffer manager");
        pthread_mutex_unlock(&g_bufMutex);
        return false;
    }
    
    g_bufMgr->persistentMappingSupported = checkPersistentMappingSupport();
    velocityLogInfo("  Persistent mapping: %s", 
                    g_bufMgr->persistentMappingSupported ? "supported" : "not supported");
    
    // Create streaming buffer
    size_t streamSize = poolSize > 0 ? poolSize : BUFFER_POOL_DEFAULT_SIZE;
    g_bufMgr->streamBufferSize = streamSize;
    
    glGenBuffers(1, &g_bufMgr->streamBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, g_bufMgr->streamBuffer);
    
    if (g_bufMgr->persistentMappingSupported && glBufferStorageEXT) {
        // Use persistent mapping for best performance
        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorageEXT(GL_ARRAY_BUFFER, streamSize, NULL, flags);
        g_bufMgr->streamMappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, streamSize, 
                                                      GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        
        if (!g_bufMgr->streamMappedPtr) {
            velocityLogWarn("Persistent mapping failed, falling back to standard");
            g_bufMgr->persistentMappingSupported = false;
        }
    }
    
    if (!g_bufMgr->persistentMappingSupported) {
        glBufferData(GL_ARRAY_BUFFER, streamSize, NULL, GL_STREAM_DRAW);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    g_bufMgr->initialized = true;
    
    velocityLogInfo("Buffer manager initialized (stream buffer: %zu KB)", 
                    streamSize / 1024);
    
    pthread_mutex_unlock(&g_bufMutex);
    return true;
}

void bufferManagerShutdown(void) {
    pthread_mutex_lock(&g_bufMutex);
    
    if (!g_bufMgr) {
        pthread_mutex_unlock(&g_bufMutex);
        return;
    }
    
    velocityLogInfo("Shutting down buffer manager");
    
    // Unmap and delete stream buffer
    if (g_bufMgr->streamMappedPtr && g_bufMgr->persistentMappingSupported) {
        glBindBuffer(GL_ARRAY_BUFFER, g_bufMgr->streamBuffer);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    glDeleteBuffers(1, &g_bufMgr->streamBuffer);
    
    // Delete sync objects
    for (int i = 0; i < 3; i++) {
        if (g_bufMgr->streamFences[i]) {
            glDeleteSync(g_bufMgr->streamFences[i]);
        }
    }
    
    // Destroy all pools
    for (int i = 0; i < g_bufMgr->poolCount; i++) {
        if (g_bufMgr->pools[i].bufferId) {
            if (g_bufMgr->pools[i].persistentMapped) {
                glBindBuffer(GL_ARRAY_BUFFER, g_bufMgr->pools[i].bufferId);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            }
            glDeleteBuffers(1, &g_bufMgr->pools[i].bufferId);
            
            // Free block list
            BufferBlock* block = g_bufMgr->pools[i].blocks;
            while (block) {
                BufferBlock* next = block->next;
                velocityFree(block);
                block = next;
            }
        }
    }
    
    velocityFree(g_bufMgr);
    g_bufMgr = NULL;
    
    pthread_mutex_unlock(&g_bufMutex);
}

// ============================================================================
// Buffer Pool Management
// ============================================================================

int bufferPoolCreate(BufferTarget target, BufferUsage usage, size_t size) {
    if (!g_bufMgr || g_bufMgr->poolCount >= MAX_BUFFER_POOLS) {
        return -1;
    }
    
    pthread_mutex_lock(&g_bufMutex);
    
    int poolIndex = g_bufMgr->poolCount;
    BufferPool* pool = &g_bufMgr->pools[poolIndex];
    
    pool->target = target;
    pool->usage = usage;
    pool->totalSize = size;
    pool->freeSize = size;
    pool->usedSize = 0;
    
    // Create GL buffer
    glGenBuffers(1, &pool->bufferId);
    glBindBuffer(target, pool->bufferId);
    
    bool usePersistent = g_bufMgr->persistentMappingSupported && 
                         glBufferStorageEXT &&
                         (usage == BUFFER_USAGE_DYNAMIC || usage == BUFFER_USAGE_STREAM);
    
    if (usePersistent) {
        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorageEXT(target, size, NULL, flags);
        pool->mappedPtr = glMapBufferRange(target, 0, size, 
                                            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        pool->persistentMapped = (pool->mappedPtr != NULL);
    } else {
        glBufferData(target, size, NULL, usage);
    }
    
    glBindBuffer(target, 0);
    
    // Initialize with single free block
    pool->blocks = (BufferBlock*)velocityMalloc(sizeof(BufferBlock));
    pool->blocks->offset = 0;
    pool->blocks->size = size;
    pool->blocks->free = true;
    pool->blocks->next = NULL;
    pool->blocks->prev = NULL;
    pool->blockCount = 1;
    
    g_bufMgr->poolCount++;
    g_bufMgr->totalAllocated += size;
    
    velocityLogInfo("Created buffer pool %d (size: %zu KB, target: 0x%x)", 
                    poolIndex, size / 1024, target);
    
    pthread_mutex_unlock(&g_bufMutex);
    
    return poolIndex;
}

void bufferPoolDestroy(int poolIndex) {
    if (!g_bufMgr || poolIndex < 0 || poolIndex >= g_bufMgr->poolCount) {
        return;
    }
    
    pthread_mutex_lock(&g_bufMutex);
    
    BufferPool* pool = &g_bufMgr->pools[poolIndex];
    
    if (pool->bufferId) {
        if (pool->persistentMapped) {
            glBindBuffer(pool->target, pool->bufferId);
            glUnmapBuffer(pool->target);
            glBindBuffer(pool->target, 0);
        }
        
        glDeleteBuffers(1, &pool->bufferId);
        
        g_bufMgr->totalAllocated -= pool->totalSize;
        
        // Free blocks
        BufferBlock* block = pool->blocks;
        while (block) {
            BufferBlock* next = block->next;
            velocityFree(block);
            block = next;
        }
        
        memset(pool, 0, sizeof(BufferPool));
    }
    
    pthread_mutex_unlock(&g_bufMutex);
}

// ============================================================================
// Pool Allocation
// ============================================================================

BufferAllocation* bufferPoolAlloc(int poolIndex, size_t size) {
    if (!g_bufMgr || poolIndex < 0 || poolIndex >= g_bufMgr->poolCount || size == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_bufMutex);
    
    BufferPool* pool = &g_bufMgr->pools[poolIndex];
    size_t alignedSize = alignSize(size, BUFFER_ALIGNMENT);
    
    // Find best-fit free block
    BufferBlock* bestBlock = NULL;
    BufferBlock* block = pool->blocks;
    
    while (block) {
        if (block->free && block->size >= alignedSize) {
            if (!bestBlock || block->size < bestBlock->size) {
                bestBlock = block;
                if (block->size == alignedSize) break;  // Perfect fit
            }
        }
        block = block->next;
    }
    
    if (!bestBlock) {
        velocityLogWarn("Buffer pool %d: no space for %zu bytes (free: %zu)", 
                        poolIndex, alignedSize, pool->freeSize);
        pthread_mutex_unlock(&g_bufMutex);
        return NULL;
    }
    
    // Split block if necessary
    if (bestBlock->size > alignedSize + BUFFER_ALIGNMENT) {
        BufferBlock* newBlock = (BufferBlock*)velocityMalloc(sizeof(BufferBlock));
        newBlock->offset = bestBlock->offset + alignedSize;
        newBlock->size = bestBlock->size - alignedSize;
        newBlock->free = true;
        newBlock->next = bestBlock->next;
        newBlock->prev = bestBlock;
        
        if (bestBlock->next) {
            bestBlock->next->prev = newBlock;
        }
        bestBlock->next = newBlock;
        bestBlock->size = alignedSize;
        pool->blockCount++;
    }
    
    bestBlock->free = false;
    pool->usedSize += bestBlock->size;
    pool->freeSize -= bestBlock->size;
    pool->allocCount++;
    
    // Create allocation handle
    BufferAllocation* alloc = (BufferAllocation*)velocityMalloc(sizeof(BufferAllocation));
    alloc->bufferId = pool->bufferId;
    alloc->offset = bestBlock->offset;
    alloc->size = size;
    alloc->alignedSize = bestBlock->size;
    alloc->poolIndex = poolIndex;
    alloc->persistent = pool->persistentMapped;
    alloc->coherent = pool->persistentMapped;
    
    if (pool->persistentMapped) {
        alloc->mappedPtr = (char*)pool->mappedPtr + bestBlock->offset;
    } else {
        alloc->mappedPtr = NULL;
    }
    
    g_bufMgr->totalUsed += bestBlock->size;
    g_bufMgr->totalAllocations++;
    
    pthread_mutex_unlock(&g_bufMutex);
    
    return alloc;
}

void bufferPoolFree(BufferAllocation* alloc) {
    if (!g_bufMgr || !alloc) return;
    
    pthread_mutex_lock(&g_bufMutex);
    
    BufferPool* pool = &g_bufMgr->pools[alloc->poolIndex];
    
    // Find the block
    BufferBlock* block = pool->blocks;
    while (block) {
        if (block->offset == alloc->offset) {
            block->free = true;
            pool->usedSize -= block->size;
            pool->freeSize += block->size;
            pool->freeCount++;
            g_bufMgr->totalUsed -= block->size;
            
            // Coalesce with next block
            if (block->next && block->next->free) {
                BufferBlock* next = block->next;
                block->size += next->size;
                block->next = next->next;
                if (next->next) {
                    next->next->prev = block;
                }
                velocityFree(next);
                pool->blockCount--;
            }
            
            // Coalesce with previous block
            if (block->prev && block->prev->free) {
                BufferBlock* prev = block->prev;
                prev->size += block->size;
                prev->next = block->next;
                if (block->next) {
                    block->next->prev = prev;
                }
                velocityFree(block);
                pool->blockCount--;
            }
            
            break;
        }
        block = block->next;
    }
    
    velocityFree(alloc);
    
    pthread_mutex_unlock(&g_bufMutex);
}

// ============================================================================
// Buffer Operations
// ============================================================================

void bufferUpload(BufferAllocation* alloc, const void* data, size_t size, size_t offset) {
    if (!alloc || !data || size == 0) return;
    
    if (offset + size > alloc->size) {
        velocityLogError("Buffer upload out of bounds");
        return;
    }
    
    if (alloc->persistent && alloc->mappedPtr) {
        // Direct copy to persistent mapping
        memcpy((char*)alloc->mappedPtr + offset, data, size);
    } else {
        // Use glBufferSubData
        BufferPool* pool = &g_bufMgr->pools[alloc->poolIndex];
        glBindBuffer(pool->target, alloc->bufferId);
        glBufferSubData(pool->target, alloc->offset + offset, size, data);
        glBindBuffer(pool->target, 0);
    }
}

void* bufferMap(BufferAllocation* alloc, size_t offset, size_t size) {
    if (!alloc) return NULL;
    
    if (alloc->persistent && alloc->mappedPtr) {
        return (char*)alloc->mappedPtr + offset;
    }
    
    BufferPool* pool = &g_bufMgr->pools[alloc->poolIndex];
    glBindBuffer(pool->target, alloc->bufferId);
    
    void* ptr = glMapBufferRange(pool->target, alloc->offset + offset, size,
                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    
    return ptr;
}

void bufferUnmap(BufferAllocation* alloc) {
    if (!alloc || alloc->persistent) return;
    
    BufferPool* pool = &g_bufMgr->pools[alloc->poolIndex];
    glBindBuffer(pool->target, alloc->bufferId);
    glUnmapBuffer(pool->target);
    glBindBuffer(pool->target, 0);
}

void bufferFlush(BufferAllocation* alloc, size_t offset, size_t size) {
    if (!alloc || !alloc->persistent) return;
    
    BufferPool* pool = &g_bufMgr->pools[alloc->poolIndex];
    glBindBuffer(pool->target, alloc->bufferId);
    glFlushMappedBufferRange(pool->target, alloc->offset + offset, size);
    glBindBuffer(pool->target, 0);
}

// ============================================================================
// Streaming Buffer
// ============================================================================

void bufferStreamBeginFrame(void) {
    if (!g_bufMgr) return;
    
    // Wait for fence from 2 frames ago (triple buffering)
    int fenceIndex = (g_bufMgr->currentFrame + 1) % 3;
    
    if (g_bufMgr->streamFences[fenceIndex]) {
        GLenum result = glClientWaitSync(g_bufMgr->streamFences[fenceIndex], 
                                          GL_SYNC_FLUSH_COMMANDS_BIT, 
                                          1000000000);  // 1 second timeout
        
        if (result == GL_TIMEOUT_EXPIRED) {
            velocityLogWarn("Stream buffer fence timeout");
        }
        
        glDeleteSync(g_bufMgr->streamFences[fenceIndex]);
        g_bufMgr->streamFences[fenceIndex] = NULL;
    }
    
    // Reset offset for this frame's portion
    size_t frameSize = g_bufMgr->streamBufferSize / 3;
    g_bufMgr->streamOffset = g_bufMgr->currentFrame * frameSize;
}

void bufferStreamEndFrame(void) {
    if (!g_bufMgr) return;
    
    // Insert fence
    g_bufMgr->streamFences[g_bufMgr->currentFrame] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    
    // Advance frame
    g_bufMgr->currentFrame = (g_bufMgr->currentFrame + 1) % 3;
}

size_t bufferStreamAlloc(size_t size, const void* data, GLuint* outBuffer) {
    if (!g_bufMgr || !outBuffer) return 0;
    
    size_t alignedSize = alignSize(size, BUFFER_ALIGNMENT);
    size_t frameSize = g_bufMgr->streamBufferSize / 3;
    size_t frameStart = g_bufMgr->currentFrame * frameSize;
    size_t frameEnd = frameStart + frameSize;
    
    // Check if we have space in current frame's region
    if (g_bufMgr->streamOffset + alignedSize > frameEnd) {
        velocityLogWarn("Stream buffer overflow for frame");
        return 0;
    }
    
    size_t offset = g_bufMgr->streamOffset;
    g_bufMgr->streamOffset += alignedSize;
    
    // Upload data
    if (data) {
        if (g_bufMgr->persistentMappingSupported && g_bufMgr->streamMappedPtr) {
            memcpy((char*)g_bufMgr->streamMappedPtr + offset, data, size);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, g_bufMgr->streamBuffer);
            glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
    
    *outBuffer = g_bufMgr->streamBuffer;
    return offset;
}

GLuint bufferStreamGetBuffer(void) {
    return g_bufMgr ? g_bufMgr->streamBuffer : 0;
}

// ============================================================================
// Direct Buffer Operations
// ============================================================================

GLuint bufferCreate(BufferTarget target, size_t size, const void* data, BufferUsage usage) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, size, data, usage);
    glBindBuffer(target, 0);
    return buffer;
}

void bufferDelete(GLuint buffer) {
    if (buffer) {
        glDeleteBuffers(1, &buffer);
    }
}

void bufferBind(BufferTarget target, GLuint buffer) {
    glBindBuffer(target, buffer);
}

void bufferBindRange(BufferTarget target, GLuint index, GLuint buffer,
                     size_t offset, size_t size) {
    glBindBufferRange(target, index, buffer, offset, size);
}

void bufferCopy(GLuint srcBuffer, GLuint dstBuffer,
                size_t srcOffset, size_t dstOffset, size_t size) {
    glBindBuffer(GL_COPY_READ_BUFFER, srcBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, dstBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffset, dstOffset, size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

// ============================================================================
// Statistics
// ============================================================================

void bufferManagerGetStats(size_t* totalAllocated, size_t* totalUsed, uint32_t* allocCount) {
    if (!g_bufMgr) {
        if (totalAllocated) *totalAllocated = 0;
        if (totalUsed) *totalUsed = 0;
        if (allocCount) *allocCount = 0;
        return;
    }
    
    pthread_mutex_lock(&g_bufMutex);
    if (totalAllocated) *totalAllocated = g_bufMgr->totalAllocated;
    if (totalUsed) *totalUsed = g_bufMgr->totalUsed;
    if (allocCount) *allocCount = g_bufMgr->totalAllocations;
    pthread_mutex_unlock(&g_bufMutex);
}

void bufferPoolDefragment(int poolIndex) {
    velocityLogInfo("Buffer pool %d defragmentation requested", poolIndex);
}

void bufferManagerTrim(void) {
    if (!g_bufMgr) return;
    velocityLogInfo("Buffer manager trim requested");
}
