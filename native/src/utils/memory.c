/**
 * VelocityGL Memory Management - Implementation
 */

#include "memory.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// ============================================================================
// Internal Structures
// ============================================================================

#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
typedef struct AllocationHeader {
    size_t size;
    const char* file;
    int line;
    struct AllocationHeader* next;
    struct AllocationHeader* prev;
    uint32_t magic;
} AllocationHeader;

#define ALLOC_MAGIC 0xDEADBEEF
#define HEADER_SIZE sizeof(AllocationHeader)
#else
#define HEADER_SIZE 0
#endif

typedef struct PoolBlock {
    struct PoolBlock* next;
    bool used;
} PoolBlock;

struct VelocityMemoryPool {
    size_t blockSize;
    size_t totalBlocks;
    size_t usedBlocks;
    PoolBlock* freeList;
    void* memory;
    size_t memorySize;
    pthread_mutex_t mutex;
};

// ============================================================================
// Global State
// ============================================================================

static struct {
    VelocityMemoryStats stats;
    pthread_mutex_t mutex;
    bool initialized;
    
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    AllocationHeader* allocList;
#endif
} g_memory = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

// ============================================================================
// Initialization
// ============================================================================

void velocityMemoryInit(void) {
    pthread_mutex_lock(&g_memory.mutex);
    
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));
    
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    g_memory.allocList = NULL;
#endif
    
    g_memory.initialized = true;
    
    pthread_mutex_unlock(&g_memory.mutex);
}

void velocityMemoryShutdown(void) {
    velocityMemoryCheckLeaks();
    
    pthread_mutex_lock(&g_memory.mutex);
    g_memory.initialized = false;
    pthread_mutex_unlock(&g_memory.mutex);
}

// ============================================================================
// Core Allocation
// ============================================================================

void* velocityMalloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t totalSize = size + HEADER_SIZE;
    void* ptr = malloc(totalSize);
    
    if (!ptr) {
        velocityLogError("Failed to allocate %zu bytes", size);
        return NULL;
    }
    
    pthread_mutex_lock(&g_memory.mutex);
    
    g_memory.stats.totalAllocated += size;
    g_memory.stats.currentUsage += size;
    g_memory.stats.allocationCount++;
    
    if (g_memory.stats.currentUsage > g_memory.stats.peakUsage) {
        g_memory.stats.peakUsage = g_memory.stats.currentUsage;
    }
    
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    AllocationHeader* header = (AllocationHeader*)ptr;
    header->size = size;
    header->magic = ALLOC_MAGIC;
    header->next = g_memory.allocList;
    header->prev = NULL;
    
    if (g_memory.allocList) {
        g_memory.allocList->prev = header;
    }
    g_memory.allocList = header;
    
    ptr = (char*)ptr + HEADER_SIZE;
#endif
    
    pthread_mutex_unlock(&g_memory.mutex);
    
    return ptr;
}

void* velocityCalloc(size_t count, size_t size) {
    size_t totalSize = count * size;
    void* ptr = velocityMalloc(totalSize);
    
    if (ptr) {
        memset(ptr, 0, totalSize);
    }
    
    return ptr;
}

void* velocityRealloc(void* ptr, size_t newSize) {
    if (!ptr) {
        return velocityMalloc(newSize);
    }
    
    if (newSize == 0) {
        velocityFree(ptr);
        return NULL;
    }
    
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    AllocationHeader* header = (AllocationHeader*)((char*)ptr - HEADER_SIZE);
    
    if (header->magic != ALLOC_MAGIC) {
        velocityLogError("Memory corruption detected in realloc");
        return NULL;
    }
    
    size_t oldSize = header->size;
    
    // Remove from list
    pthread_mutex_lock(&g_memory.mutex);
    
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        g_memory.allocList = header->next;
    }
    
    if (header->next) {
        header->next->prev = header->prev;
    }
    
    g_memory.stats.currentUsage -= oldSize;
    
    pthread_mutex_unlock(&g_memory.mutex);
    
    // Reallocate
    void* newPtr = realloc(header, newSize + HEADER_SIZE);
    if (!newPtr) {
        // Restore to list on failure
        pthread_mutex_lock(&g_memory.mutex);
        header->next = g_memory.allocList;
        header->prev = NULL;
        if (g_memory.allocList) g_memory.allocList->prev = header;
        g_memory.allocList = header;
        g_memory.stats.currentUsage += oldSize;
        pthread_mutex_unlock(&g_memory.mutex);
        return NULL;
    }
    
    // Update and add back to list
    header = (AllocationHeader*)newPtr;
    header->size = newSize;
    
    pthread_mutex_lock(&g_memory.mutex);
    
    header->next = g_memory.allocList;
    header->prev = NULL;
    if (g_memory.allocList) g_memory.allocList->prev = header;
    g_memory.allocList = header;
    
    g_memory.stats.currentUsage += newSize;
    g_memory.stats.totalAllocated += newSize;
    
    if (g_memory.stats.currentUsage > g_memory.stats.peakUsage) {
        g_memory.stats.peakUsage = g_memory.stats.currentUsage;
    }
    
    pthread_mutex_unlock(&g_memory.mutex);
    
    return (char*)newPtr + HEADER_SIZE;
#else
    return realloc(ptr, newSize);
#endif
}

void velocityFree(void* ptr) {
    if (!ptr) return;
    
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    AllocationHeader* header = (AllocationHeader*)((char*)ptr - HEADER_SIZE);
    
    if (header->magic != ALLOC_MAGIC) {
        velocityLogError("Memory corruption detected in free: invalid magic");
        return;
    }
    
    pthread_mutex_lock(&g_memory.mutex);
    
    // Remove from list
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        g_memory.allocList = header->next;
    }
    
    if (header->next) {
        header->next->prev = header->prev;
    }
    
    g_memory.stats.totalFreed += header->size;
    g_memory.stats.currentUsage -= header->size;
    g_memory.stats.freeCount++;
    
    header->magic = 0; // Invalidate
    
    pthread_mutex_unlock(&g_memory.mutex);
    
    free(header);
#else
    pthread_mutex_lock(&g_memory.mutex);
    g_memory.stats.freeCount++;
    pthread_mutex_unlock(&g_memory.mutex);
    
    free(ptr);
#endif
}

void* velocityAlignedMalloc(size_t size, size_t alignment) {
    void* ptr = NULL;
    
    if (posix_memalign(&ptr, alignment, size) != 0) {
        velocityLogError("Failed to allocate aligned memory: %zu bytes, alignment %zu", size, alignment);
        return NULL;
    }
    
    pthread_mutex_lock(&g_memory.mutex);
    g_memory.stats.totalAllocated += size;
    g_memory.stats.currentUsage += size;
    g_memory.stats.allocationCount++;
    pthread_mutex_unlock(&g_memory.mutex);
    
    return ptr;
}

void velocityAlignedFree(void* ptr) {
    if (ptr) {
        free(ptr);
        
        pthread_mutex_lock(&g_memory.mutex);
        g_memory.stats.freeCount++;
        pthread_mutex_unlock(&g_memory.mutex);
    }
}

char* velocityStrdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* dup = (char*)velocityMalloc(len);
    
    if (dup) {
        memcpy(dup, str, len);
    }
    
    return dup;
}

char* velocityStrndup(const char* str, size_t maxLen) {
    if (!str) return NULL;
    
    size_t len = strnlen(str, maxLen);
    char* dup = (char*)velocityMalloc(len + 1);
    
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    
    return dup;
}

// ============================================================================
// Memory Pool
// ============================================================================

VelocityMemoryPool* velocityPoolCreate(size_t blockSize, size_t initialBlocks) {
    VelocityMemoryPool* pool = (VelocityMemoryPool*)velocityMalloc(sizeof(VelocityMemoryPool));
    if (!pool) return NULL;
    
    // Ensure block size can hold PoolBlock header
    if (blockSize < sizeof(PoolBlock)) {
        blockSize = sizeof(PoolBlock);
    }
    
    pool->blockSize = blockSize;
    pool->totalBlocks = initialBlocks;
    pool->usedBlocks = 0;
    pool->memorySize = blockSize * initialBlocks;
    
    pool->memory = velocityMalloc(pool->memorySize);
    if (!pool->memory) {
        velocityFree(pool);
        return NULL;
    }
    
    pthread_mutex_init(&pool->mutex, NULL);
    
    // Initialize free list
    pool->freeList = NULL;
    char* ptr = (char*)pool->memory;
    
    for (size_t i = 0; i < initialBlocks; i++) {
        PoolBlock* block = (PoolBlock*)ptr;
        block->next = pool->freeList;
        block->used = false;
        pool->freeList = block;
        ptr += blockSize;
    }
    
    return pool;
}

void velocityPoolDestroy(VelocityMemoryPool* pool) {
    if (!pool) return;
    
    pthread_mutex_destroy(&pool->mutex);
    velocityFree(pool->memory);
    velocityFree(pool);
}

void* velocityPoolAlloc(VelocityMemoryPool* pool) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->mutex);
    
    if (!pool->freeList) {
        // Pool exhausted, fall back to regular alloc
        pthread_mutex_unlock(&pool->mutex);
        g_memory.stats.poolMisses++;
        return velocityMalloc(pool->blockSize);
    }
    
    PoolBlock* block = pool->freeList;
    pool->freeList = block->next;
    block->used = true;
    pool->usedBlocks++;
    
    g_memory.stats.poolHits++;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return (void*)block;
}

void velocityPoolFree(VelocityMemoryPool* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    // Check if pointer is within pool memory
    char* ptrChar = (char*)ptr;
    char* poolStart = (char*)pool->memory;
    char* poolEnd = poolStart + pool->memorySize;
    
    if (ptrChar >= poolStart && ptrChar < poolEnd) {
        pthread_mutex_lock(&pool->mutex);
        
        PoolBlock* block = (PoolBlock*)ptr;
        block->next = pool->freeList;
        block->used = false;
        pool->freeList = block;
        pool->usedBlocks--;
        
        pthread_mutex_unlock(&pool->mutex);
    } else {
        // Not from pool, use regular free
        velocityFree(ptr);
    }
}

void velocityPoolReset(VelocityMemoryPool* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    // Rebuild free list
    pool->freeList = NULL;
    pool->usedBlocks = 0;
    
    char* ptr = (char*)pool->memory;
    for (size_t i = 0; i < pool->totalBlocks; i++) {
        PoolBlock* block = (PoolBlock*)ptr;
        block->next = pool->freeList;
        block->used = false;
        pool->freeList = block;
        ptr += pool->blockSize;
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

void velocityPoolGetStats(VelocityMemoryPool* pool, size_t* used, size_t* total) {
    if (!pool) {
        if (used) *used = 0;
        if (total) *total = 0;
        return;
    }
    
    pthread_mutex_lock(&pool->mutex);
    if (used) *used = pool->usedBlocks;
    if (total) *total = pool->totalBlocks;
    pthread_mutex_unlock(&pool->mutex);
}

// ============================================================================
// Ring Buffer
// ============================================================================

VelocityRingBuffer* velocityRingBufferCreate(size_t size) {
    VelocityRingBuffer* rb = (VelocityRingBuffer*)velocityMalloc(sizeof(VelocityRingBuffer));
    if (!rb) return NULL;
    
    rb->data = velocityAlignedMalloc(size, VELOCITY_MEMORY_ALIGNMENT);
    if (!rb->data) {
        velocityFree(rb);
        return NULL;
    }
    
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->used = 0;
    
    return rb;
}

void velocityRingBufferDestroy(VelocityRingBuffer* rb) {
    if (!rb) return;
    
    velocityAlignedFree(rb->data);
    velocityFree(rb);
}

void* velocityRingBufferAlloc(VelocityRingBuffer* rb, size_t size, size_t* offset) {
    if (!rb || size == 0 || size > rb->size) return NULL;
    
    // Align size
    size = (size + VELOCITY_MEMORY_ALIGNMENT - 1) & ~(VELOCITY_MEMORY_ALIGNMENT - 1);
    
    // Check if we need to wrap
    if (rb->head + size > rb->size) {
        // Waste remaining space and wrap
        rb->head = 0;
    }
    
    // Check if enough space
    if (rb->used + size > rb->size) {
        return NULL;
    }
    
    void* ptr = (char*)rb->data + rb->head;
    if (offset) *offset = rb->head;
    
    rb->head = (rb->head + size) % rb->size;
    rb->used += size;
    
    return ptr;
}

void velocityRingBufferReset(VelocityRingBuffer* rb) {
    if (!rb) return;
    
    rb->head = 0;
    rb->tail = 0;
    rb->used = 0;
}

// ============================================================================
// Statistics
// ============================================================================

VelocityMemoryStats velocityMemoryGetStats(void) {
    pthread_mutex_lock(&g_memory.mutex);
    VelocityMemoryStats stats = g_memory.stats;
    pthread_mutex_unlock(&g_memory.mutex);
    return stats;
}

void velocityMemoryResetStats(void) {
    pthread_mutex_lock(&g_memory.mutex);
    
    size_t current = g_memory.stats.currentUsage;
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));
    g_memory.stats.currentUsage = current;
    g_memory.stats.peakUsage = current;
    
    pthread_mutex_unlock(&g_memory.mutex);
}

size_t velocityMemoryGetUsage(void) {
    pthread_mutex_lock(&g_memory.mutex);
    size_t usage = g_memory.stats.currentUsage;
    pthread_mutex_unlock(&g_memory.mutex);
    return usage;
}

void velocityMemoryCheckLeaks(void) {
#if VELOCITY_MEMORY_TRACK_ALLOCATIONS
    pthread_mutex_lock(&g_memory.mutex);
    
    if (g_memory.allocList) {
        velocityLogWarn("=== Memory Leak Report ===");
        
        AllocationHeader* header = g_memory.allocList;
        int count = 0;
        size_t totalLeaked = 0;
        
        while (header && count < 20) {
            velocityLogWarn("  Leak: %zu bytes", header->size);
            totalLeaked += header->size;
            header = header->next;
            count++;
        }
        
        if (header) {
            velocityLogWarn("  ... and more");
        }
        
        velocityLogWarn("Total leaked: %zu bytes", totalLeaked);
    } else {
        velocityLogInfo("No memory leaks detected");
    }
    
    pthread_mutex_unlock(&g_memory.mutex);
#endif
}

void velocityMemoryTrim(void) {
    // Could implement pool trimming here
    velocityLogDebug("Memory trim requested");
}
