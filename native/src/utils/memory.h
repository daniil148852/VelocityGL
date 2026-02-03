/**
 * VelocityGL Memory Management
 * Custom allocator with tracking and pooling
 */

#ifndef VELOCITY_MEMORY_H
#define VELOCITY_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#define VELOCITY_MEMORY_ALIGNMENT 16
#define VELOCITY_MEMORY_POOL_BLOCK_SIZE (64 * 1024)  // 64KB blocks
#define VELOCITY_MEMORY_TRACK_ALLOCATIONS 1

// ============================================================================
// Memory Statistics
// ============================================================================

typedef struct VelocityMemoryStats {
    size_t totalAllocated;
    size_t totalFreed;
    size_t currentUsage;
    size_t peakUsage;
    size_t allocationCount;
    size_t freeCount;
    size_t poolHits;
    size_t poolMisses;
} VelocityMemoryStats;

// ============================================================================
// Core Allocation Functions
// ============================================================================

/**
 * Initialize memory system
 */
void velocityMemoryInit(void);

/**
 * Shutdown memory system
 */
void velocityMemoryShutdown(void);

/**
 * Allocate memory
 */
void* velocityMalloc(size_t size);

/**
 * Allocate zeroed memory
 */
void* velocityCalloc(size_t count, size_t size);

/**
 * Reallocate memory
 */
void* velocityRealloc(void* ptr, size_t newSize);

/**
 * Free memory
 */
void velocityFree(void* ptr);

/**
 * Allocate aligned memory
 */
void* velocityAlignedMalloc(size_t size, size_t alignment);

/**
 * Free aligned memory
 */
void velocityAlignedFree(void* ptr);

/**
 * Duplicate string
 */
char* velocityStrdup(const char* str);

/**
 * Duplicate string with max length
 */
char* velocityStrndup(const char* str, size_t maxLen);

// ============================================================================
// Memory Pool
// ============================================================================

/**
 * Pool handle
 */
typedef struct VelocityMemoryPool VelocityMemoryPool;

/**
 * Create memory pool
 * @param blockSize Size of each block
 * @param initialBlocks Number of blocks to pre-allocate
 */
VelocityMemoryPool* velocityPoolCreate(size_t blockSize, size_t initialBlocks);

/**
 * Destroy memory pool
 */
void velocityPoolDestroy(VelocityMemoryPool* pool);

/**
 * Allocate from pool
 */
void* velocityPoolAlloc(VelocityMemoryPool* pool);

/**
 * Return to pool
 */
void velocityPoolFree(VelocityMemoryPool* pool, void* ptr);

/**
 * Reset pool (free all allocations)
 */
void velocityPoolReset(VelocityMemoryPool* pool);

/**
 * Get pool statistics
 */
void velocityPoolGetStats(VelocityMemoryPool* pool, size_t* used, size_t* total);

// ============================================================================
// Ring Buffer
// ============================================================================

/**
 * Ring buffer for streaming data
 */
typedef struct VelocityRingBuffer {
    void* data;
    size_t size;
    size_t head;
    size_t tail;
    size_t used;
} VelocityRingBuffer;

/**
 * Create ring buffer
 */
VelocityRingBuffer* velocityRingBufferCreate(size_t size);

/**
 * Destroy ring buffer
 */
void velocityRingBufferDestroy(VelocityRingBuffer* rb);

/**
 * Allocate space from ring buffer
 * Returns NULL if not enough space
 */
void* velocityRingBufferAlloc(VelocityRingBuffer* rb, size_t size, size_t* offset);

/**
 * Reset ring buffer
 */
void velocityRingBufferReset(VelocityRingBuffer* rb);

// ============================================================================
// Statistics & Debugging
// ============================================================================

/**
 * Get memory statistics
 */
VelocityMemoryStats velocityMemoryGetStats(void);

/**
 * Reset statistics
 */
void velocityMemoryResetStats(void);

/**
 * Trim memory (release unused pools)
 */
void velocityMemoryTrim(void);

/**
 * Check for memory leaks
 */
void velocityMemoryCheckLeaks(void);

/**
 * Get current memory usage
 */
size_t velocityMemoryGetUsage(void);

#ifdef __cplusplus
}
#endif

#endif // VELOCITY_MEMORY_H
