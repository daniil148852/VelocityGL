/**
 * Shader Cache - Binary shader caching for fast loading
 */

#ifndef SHADER_CACHE_H
#define SHADER_CACHE_H

#include <GLES3/gl32.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Constants
// ============================================================================

#define SHADER_CACHE_MAGIC 0x56454C53  // "VELS"
#define SHADER_CACHE_VERSION 1
#define MAX_SHADER_SOURCE_HASH 64
#define MAX_CACHED_PROGRAMS 256

// ============================================================================
// Types
// ============================================================================

/**
 * Shader type
 */
typedef enum ShaderType {
    SHADER_TYPE_VERTEX = GL_VERTEX_SHADER,
    SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
    SHADER_TYPE_GEOMETRY = 0x8DD9,  // GL_GEOMETRY_SHADER
    SHADER_TYPE_COMPUTE = GL_COMPUTE_SHADER
} ShaderType;

/**
 * Cache entry header (stored on disk)
 */
typedef struct ShaderCacheHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t gpuVendorHash;       // To invalidate on GPU change
    uint32_t driverVersionHash;
    uint64_t timestamp;
    uint32_t entryCount;
    uint32_t reserved;
} ShaderCacheHeader;

/**
 * Single cached shader entry
 */
typedef struct ShaderCacheEntry {
    uint64_t sourceHash;          // Hash of original source
    GLenum binaryFormat;
    uint32_t binarySize;
    uint32_t dataOffset;          // Offset in cache file
    bool isProgram;               // true = linked program, false = single shader
    uint8_t shaderTypes;          // Bitmask of shader types in program
} ShaderCacheEntry;

/**
 * In-memory cache entry
 */
typedef struct MemoryCacheEntry {
    uint64_t hash;
    GLuint programId;
    void* binaryData;
    uint32_t binarySize;
    GLenum binaryFormat;
    int hitCount;
    uint64_t lastUsed;
    bool dirty;                   // Needs to be saved to disk
} MemoryCacheEntry;

/**
 * Shader cache context
 */
typedef struct ShaderCacheContext {
    // Configuration
    char* cachePath;
    size_t maxCacheSize;
    
    // Memory cache
    MemoryCacheEntry* entries;
    int entryCount;
    int maxEntries;
    
    // Statistics
    uint32_t hits;
    uint32_t misses;
    size_t totalSize;
    
    // State
    bool initialized;
    bool diskCacheEnabled;
    
    // GPU info for cache validation
    uint32_t gpuVendorHash;
    uint32_t driverVersionHash;
    
} ShaderCacheContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize shader cache
 * @param cachePath Directory for disk cache (NULL for memory only)
 * @param maxSize Maximum cache size in bytes
 */
bool shaderCacheInit(const char* cachePath, size_t maxSize);

/**
 * Shutdown shader cache, save to disk
 */
void shaderCacheShutdown(void);

/**
 * Clear all cached shaders
 */
void shaderCacheClear(void);

/**
 * Flush dirty entries to disk
 */
void shaderCacheFlush(void);

/**
 * Try to get cached program binary
 * @param vertSource Vertex shader source
 * @param fragSource Fragment shader source
 * @param outProgram Output program ID if found
 * @return true if cache hit
 */
bool shaderCacheGetProgram(const char* vertSource, const char* fragSource, GLuint* outProgram);

/**
 * Store compiled program in cache
 * @param vertSource Vertex shader source
 * @param fragSource Fragment shader source
 * @param program Compiled program ID
 */
void shaderCacheStoreProgram(const char* vertSource, const char* fragSource, GLuint program);

/**
 * Get cache statistics
 */
void shaderCacheGetStats(uint32_t* hits, uint32_t* misses, size_t* size);

/**
 * Preload common Minecraft shaders
 */
void shaderCachePreload(void);

/**
 * Compute hash of shader source
 */
uint64_t shaderCacheHashSource(const char* source);

/**
 * Compute combined hash for program
 */
uint64_t shaderCacheHashProgram(const char* vertSource, const char* fragSource);

// ============================================================================
// Internal Functions
// ============================================================================

/**
 * Load cache from disk
 */
bool shaderCacheLoadFromDisk(void);

/**
 * Save cache to disk
 */
bool shaderCacheSaveToDisk(void);

/**
 * Evict LRU entries to make space
 */
void shaderCacheEvict(size_t bytesNeeded);

/**
 * Find entry by hash
 */
MemoryCacheEntry* shaderCacheFindEntry(uint64_t hash);

/**
 * Create program from binary
 */
GLuint shaderCacheCreateProgramFromBinary(GLenum format, const void* binary, GLsizei length);

/**
 * Get program binary
 */
bool shaderCacheGetProgramBinary(GLuint program, GLenum* format, void** binary, GLsizei* length);

#endif // SHADER_CACHE_H
