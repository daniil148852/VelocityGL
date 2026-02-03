/**
 * Shader Cache - Implementation
 */

#include "shader_cache.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../utils/hash.h"
#include "../core/gl_wrapper.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

// ============================================================================
// Global State
// ============================================================================

static ShaderCacheContext* g_shaderCache = NULL;

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t getCurrentTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

static bool ensureDirectoryExists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    // Create directory
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    
    velocityLogError("Failed to create cache directory: %s (errno=%d)", path, errno);
    return false;
}

// ============================================================================
// Hash Functions
// ============================================================================

uint64_t shaderCacheHashSource(const char* source) {
    if (!source) return 0;
    
    // Use FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    while (*source) {
        hash ^= (uint8_t)*source++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t shaderCacheHashProgram(const char* vertSource, const char* fragSource) {
    uint64_t vertHash = shaderCacheHashSource(vertSource);
    uint64_t fragHash = shaderCacheHashSource(fragSource);
    
    // Combine hashes
    return vertHash ^ (fragHash * 31);
}

// ============================================================================
// Initialization
// ============================================================================

bool shaderCacheInit(const char* cachePath, size_t maxSize) {
    if (g_shaderCache) {
        velocityLogWarn("Shader cache already initialized");
        return true;
    }
    
    velocityLogInfo("Initializing shader cache (max size: %zu MB)", maxSize / (1024 * 1024));
    
    g_shaderCache = (ShaderCacheContext*)velocityMalloc(sizeof(ShaderCacheContext));
    if (!g_shaderCache) {
        velocityLogError("Failed to allocate shader cache context");
        return false;
    }
    
    memset(g_shaderCache, 0, sizeof(ShaderCacheContext));
    
    // Configuration
    g_shaderCache->maxCacheSize = maxSize > 0 ? maxSize : (64 * 1024 * 1024); // Default 64MB
    g_shaderCache->maxEntries = MAX_CACHED_PROGRAMS;
    
    // Allocate entries
    g_shaderCache->entries = (MemoryCacheEntry*)velocityMalloc(
        sizeof(MemoryCacheEntry) * g_shaderCache->maxEntries);
    if (!g_shaderCache->entries) {
        velocityLogError("Failed to allocate cache entries");
        velocityFree(g_shaderCache);
        g_shaderCache = NULL;
        return false;
    }
    
    memset(g_shaderCache->entries, 0, sizeof(MemoryCacheEntry) * g_shaderCache->maxEntries);
    
    // Set up disk cache if path provided
    if (cachePath && cachePath[0] != '\0') {
        g_shaderCache->cachePath = velocityStrdup(cachePath);
        
        if (ensureDirectoryExists(cachePath)) {
            g_shaderCache->diskCacheEnabled = true;
            
            // Try to load existing cache
            shaderCacheLoadFromDisk();
        }
    }
    
    // Compute GPU hash for cache validation
    if (g_wrapperCtx) {
        g_shaderCache->gpuVendorHash = shaderCacheHashSource(g_wrapperCtx->gpuCaps.rendererString);
        g_shaderCache->driverVersionHash = shaderCacheHashSource(g_wrapperCtx->gpuCaps.versionString);
    }
    
    g_shaderCache->initialized = true;
    
    velocityLogInfo("Shader cache initialized (%d entries from disk)", g_shaderCache->entryCount);
    return true;
}

void shaderCacheShutdown(void) {
    if (!g_shaderCache) return;
    
    velocityLogInfo("Shutting down shader cache (hits: %u, misses: %u)", 
                    g_shaderCache->hits, g_shaderCache->misses);
    
    // Save to disk before shutdown
    if (g_shaderCache->diskCacheEnabled) {
        shaderCacheSaveToDisk();
    }
    
    // Free entries
    for (int i = 0; i < g_shaderCache->entryCount; i++) {
        if (g_shaderCache->entries[i].binaryData) {
            velocityFree(g_shaderCache->entries[i].binaryData);
        }
    }
    
    velocityFree(g_shaderCache->entries);
    velocityFree(g_shaderCache->cachePath);
    velocityFree(g_shaderCache);
    g_shaderCache = NULL;
}

void shaderCacheClear(void) {
    if (!g_shaderCache) return;
    
    for (int i = 0; i < g_shaderCache->entryCount; i++) {
        if (g_shaderCache->entries[i].binaryData) {
            velocityFree(g_shaderCache->entries[i].binaryData);
        }
    }
    
    memset(g_shaderCache->entries, 0, sizeof(MemoryCacheEntry) * g_shaderCache->maxEntries);
    g_shaderCache->entryCount = 0;
    g_shaderCache->totalSize = 0;
    g_shaderCache->hits = 0;
    g_shaderCache->misses = 0;
    
    velocityLogInfo("Shader cache cleared");
}

// ============================================================================
// Cache Lookup
// ============================================================================

MemoryCacheEntry* shaderCacheFindEntry(uint64_t hash) {
    if (!g_shaderCache) return NULL;
    
    for (int i = 0; i < g_shaderCache->entryCount; i++) {
        if (g_shaderCache->entries[i].hash == hash) {
            return &g_shaderCache->entries[i];
        }
    }
    return NULL;
}

bool shaderCacheGetProgram(const char* vertSource, const char* fragSource, GLuint* outProgram) {
    if (!g_shaderCache || !vertSource || !fragSource || !outProgram) {
        return false;
    }
    
    uint64_t hash = shaderCacheHashProgram(vertSource, fragSource);
    MemoryCacheEntry* entry = shaderCacheFindEntry(hash);
    
    if (!entry || !entry->binaryData) {
        g_shaderCache->misses++;
        return false;
    }
    
    // Try to create program from binary
    GLuint program = shaderCacheCreateProgramFromBinary(
        entry->binaryFormat, 
        entry->binaryData, 
        entry->binarySize
    );
    
    if (program == 0) {
        // Binary is invalid, remove from cache
        velocityLogWarn("Cached shader binary invalid, removing");
        velocityFree(entry->binaryData);
        entry->binaryData = NULL;
        entry->hash = 0;
        g_shaderCache->misses++;
        return false;
    }
    
    // Update statistics
    entry->hitCount++;
    entry->lastUsed = getCurrentTime();
    g_shaderCache->hits++;
    
    *outProgram = program;
    
    velocityLogDebug("Shader cache hit (hash: 0x%llx)", (unsigned long long)hash);
    return true;
}

// ============================================================================
// Cache Storage
// ============================================================================

void shaderCacheStoreProgram(const char* vertSource, const char* fragSource, GLuint program) {
    if (!g_shaderCache || !vertSource || !fragSource || program == 0) {
        return;
    }
    
    uint64_t hash = shaderCacheHashProgram(vertSource, fragSource);
    
    // Check if already cached
    if (shaderCacheFindEntry(hash)) {
        return;
    }
    
    // Get program binary
    GLenum format;
    void* binary;
    GLsizei length;
    
    if (!shaderCacheGetProgramBinary(program, &format, &binary, &length)) {
        velocityLogWarn("Failed to get program binary");
        return;
    }
    
    // Check if we need to evict
    if (g_shaderCache->totalSize + length > g_shaderCache->maxCacheSize ||
        g_shaderCache->entryCount >= g_shaderCache->maxEntries) {
        shaderCacheEvict(length);
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < g_shaderCache->maxEntries; i++) {
        if (g_shaderCache->entries[i].hash == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        velocityLogWarn("No free cache slots");
        velocityFree(binary);
        return;
    }
    
    // Store entry
    MemoryCacheEntry* entry = &g_shaderCache->entries[slot];
    entry->hash = hash;
    entry->programId = program;
    entry->binaryData = binary;
    entry->binarySize = length;
    entry->binaryFormat = format;
    entry->hitCount = 0;
    entry->lastUsed = getCurrentTime();
    entry->dirty = true;
    
    g_shaderCache->totalSize += length;
    if (slot >= g_shaderCache->entryCount) {
        g_shaderCache->entryCount = slot + 1;
    }
    
    velocityLogDebug("Cached shader program (hash: 0x%llx, size: %d)", 
                     (unsigned long long)hash, length);
}

// ============================================================================
// Program Binary Operations
// ============================================================================

GLuint shaderCacheCreateProgramFromBinary(GLenum format, const void* binary, GLsizei length) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        return 0;
    }
    
    glProgramBinary(program, format, binary, length);
    
    // Check link status
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    
    if (status != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

bool shaderCacheGetProgramBinary(GLuint program, GLenum* format, void** binary, GLsizei* length) {
    // Get binary length
    GLint binaryLength = 0;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    
    if (binaryLength <= 0) {
        return false;
    }
    
    // Allocate buffer
    void* binaryData = velocityMalloc(binaryLength);
    if (!binaryData) {
        return false;
    }
    
    // Get binary
    GLenum binaryFormat;
    GLsizei actualLength;
    glGetProgramBinary(program, binaryLength, &actualLength, &binaryFormat, binaryData);
    
    GLenum err = glGetError();
    if (err != GL_NO_ERROR || actualLength <= 0) {
        velocityFree(binaryData);
        return false;
    }
    
    *format = binaryFormat;
    *binary = binaryData;
    *length = actualLength;
    
    return true;
}

// ============================================================================
// Cache Eviction
// ============================================================================

void shaderCacheEvict(size_t bytesNeeded) {
    if (!g_shaderCache) return;
    
    velocityLogDebug("Evicting cache entries (need %zu bytes)", bytesNeeded);
    
    // Find LRU entries and evict
    while (g_shaderCache->totalSize + bytesNeeded > g_shaderCache->maxCacheSize &&
           g_shaderCache->entryCount > 0) {
        
        // Find least recently used
        int lruIndex = -1;
        uint64_t oldestTime = UINT64_MAX;
        
        for (int i = 0; i < g_shaderCache->maxEntries; i++) {
            if (g_shaderCache->entries[i].hash != 0 &&
                g_shaderCache->entries[i].lastUsed < oldestTime) {
                oldestTime = g_shaderCache->entries[i].lastUsed;
                lruIndex = i;
            }
        }
        
        if (lruIndex < 0) break;
        
        // Evict
        MemoryCacheEntry* entry = &g_shaderCache->entries[lruIndex];
        g_shaderCache->totalSize -= entry->binarySize;
        velocityFree(entry->binaryData);
        memset(entry, 0, sizeof(MemoryCacheEntry));
    }
}

// ============================================================================
// Disk Cache
// ============================================================================

bool shaderCacheLoadFromDisk(void) {
    if (!g_shaderCache || !g_shaderCache->diskCacheEnabled) {
        return false;
    }
    
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/shader_cache.bin", g_shaderCache->cachePath);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        velocityLogDebug("No existing shader cache file");
        return false;
    }
    
    // Read header
    ShaderCacheHeader header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Validate header
    if (header.magic != SHADER_CACHE_MAGIC ||
        header.version != SHADER_CACHE_VERSION ||
        header.gpuVendorHash != g_shaderCache->gpuVendorHash) {
        velocityLogInfo("Shader cache invalidated (GPU or version changed)");
        fclose(file);
        return false;
    }
    
    // Read entries
    for (uint32_t i = 0; i < header.entryCount && i < (uint32_t)g_shaderCache->maxEntries; i++) {
        ShaderCacheEntry diskEntry;
        if (fread(&diskEntry, sizeof(diskEntry), 1, file) != 1) {
            break;
        }
        
        // Allocate and read binary data
        void* binaryData = velocityMalloc(diskEntry.binarySize);
        if (!binaryData) continue;
        
        long currentPos = ftell(file);
        fseek(file, diskEntry.dataOffset, SEEK_SET);
        if (fread(binaryData, 1, diskEntry.binarySize, file) != diskEntry.binarySize) {
            velocityFree(binaryData);
            fseek(file, currentPos, SEEK_SET);
            continue;
        }
        fseek(file, currentPos, SEEK_SET);
        
        // Store in memory cache
        MemoryCacheEntry* entry = &g_shaderCache->entries[g_shaderCache->entryCount];
        entry->hash = diskEntry.sourceHash;
        entry->binaryData = binaryData;
        entry->binarySize = diskEntry.binarySize;
        entry->binaryFormat = diskEntry.binaryFormat;
        entry->lastUsed = getCurrentTime();
        entry->dirty = false;
        
        g_shaderCache->totalSize += diskEntry.binarySize;
        g_shaderCache->entryCount++;
    }
    
    fclose(file);
    
    velocityLogInfo("Loaded %d cached shaders from disk", g_shaderCache->entryCount);
    return true;
}

bool shaderCacheSaveToDisk(void) {
    if (!g_shaderCache || !g_shaderCache->diskCacheEnabled) {
        return false;
    }
    
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/shader_cache.bin", g_shaderCache->cachePath);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        velocityLogError("Failed to open shader cache for writing");
        return false;
    }
    
    // Write header
    ShaderCacheHeader header = {
        .magic = SHADER_CACHE_MAGIC,
        .version = SHADER_CACHE_VERSION,
        .gpuVendorHash = g_shaderCache->gpuVendorHash,
        .driverVersionHash = g_shaderCache->driverVersionHash,
        .timestamp = (uint64_t)time(NULL),
        .entryCount = 0,
        .reserved = 0
    };
    
    // Count valid entries
    for (int i = 0; i < g_shaderCache->maxEntries; i++) {
        if (g_shaderCache->entries[i].hash != 0 && g_shaderCache->entries[i].binaryData) {
            header.entryCount++;
        }
    }
    
    fwrite(&header, sizeof(header), 1, file);
    
    // Calculate data offset (after all entry headers)
    uint32_t dataOffset = sizeof(header) + header.entryCount * sizeof(ShaderCacheEntry);
    
    // First pass: write entry headers
    for (int i = 0; i < g_shaderCache->maxEntries; i++) {
        MemoryCacheEntry* mem = &g_shaderCache->entries[i];
        if (mem->hash == 0 || !mem->binaryData) continue;
        
        ShaderCacheEntry diskEntry = {
            .sourceHash = mem->hash,
            .binaryFormat = mem->binaryFormat,
            .binarySize = mem->binarySize,
            .dataOffset = dataOffset,
            .isProgram = true,
            .shaderTypes = 0x03  // vertex + fragment
        };
        
        fwrite(&diskEntry, sizeof(diskEntry), 1, file);
        dataOffset += mem->binarySize;
    }
    
    // Second pass: write binary data
    for (int i = 0; i < g_shaderCache->maxEntries; i++) {
        MemoryCacheEntry* mem = &g_shaderCache->entries[i];
        if (mem->hash == 0 || !mem->binaryData) continue;
        
        fwrite(mem->binaryData, 1, mem->binarySize, file);
        mem->dirty = false;
    }
    
    fclose(file);
    
    velocityLogInfo("Saved %u shaders to disk cache", header.entryCount);
    return true;
}

void shaderCacheFlush(void) {
    if (g_shaderCache && g_shaderCache->diskCacheEnabled) {
        shaderCacheSaveToDisk();
    }
}

// ============================================================================
// Statistics
// ============================================================================

void shaderCacheGetStats(uint32_t* hits, uint32_t* misses, size_t* size) {
    if (!g_shaderCache) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (size) *size = 0;
        return;
    }
    
    if (hits) *hits = g_shaderCache->hits;
    if (misses) *misses = g_shaderCache->misses;
    if (size) *size = g_shaderCache->totalSize;
}
