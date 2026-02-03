/**
 * Texture Manager - Texture loading, caching and optimization
 */

#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

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

#define MAX_TEXTURE_POOL_SIZE 512
#define TEXTURE_CACHE_MAGIC 0x56544558  // "VTEX"
#define DEFAULT_ANISOTROPY 4.0f

// ============================================================================
// Types
// ============================================================================

/**
 * Texture format
 */
typedef enum TextureFormat {
    TEX_FORMAT_UNKNOWN = 0,
    TEX_FORMAT_RGBA8,
    TEX_FORMAT_RGB8,
    TEX_FORMAT_RGBA16F,
    TEX_FORMAT_RGB16F,
    TEX_FORMAT_R8,
    TEX_FORMAT_RG8,
    TEX_FORMAT_DEPTH24,
    TEX_FORMAT_DEPTH32F,
    TEX_FORMAT_DEPTH24_STENCIL8,
    TEX_FORMAT_ETC2_RGB,
    TEX_FORMAT_ETC2_RGBA,
    TEX_FORMAT_ASTC_4x4,
    TEX_FORMAT_ASTC_6x6,
    TEX_FORMAT_ASTC_8x8
} TextureFormat;

/**
 * Texture type
 */
typedef enum TextureType {
    TEX_TYPE_2D = GL_TEXTURE_2D,
    TEX_TYPE_3D = GL_TEXTURE_3D,
    TEX_TYPE_CUBE = GL_TEXTURE_CUBE_MAP,
    TEX_TYPE_2D_ARRAY = GL_TEXTURE_2D_ARRAY
} TextureType;

/**
 * Texture wrap mode
 */
typedef enum TextureWrap {
    TEX_WRAP_REPEAT = GL_REPEAT,
    TEX_WRAP_CLAMP = GL_CLAMP_TO_EDGE,
    TEX_WRAP_MIRROR = GL_MIRRORED_REPEAT
} TextureWrap;

/**
 * Texture filter mode
 */
typedef enum TextureFilter {
    TEX_FILTER_NEAREST = GL_NEAREST,
    TEX_FILTER_LINEAR = GL_LINEAR,
    TEX_FILTER_NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST,
    TEX_FILTER_LINEAR_MIPMAP_NEAREST = GL_LINEAR_MIPMAP_NEAREST,
    TEX_FILTER_NEAREST_MIPMAP_LINEAR = GL_NEAREST_MIPMAP_LINEAR,
    TEX_FILTER_LINEAR_MIPMAP_LINEAR = GL_LINEAR_MIPMAP_LINEAR
} TextureFilter;

/**
 * Texture creation parameters
 */
typedef struct TextureParams {
    TextureType type;
    TextureFormat format;
    int width;
    int height;
    int depth;              // For 3D textures
    int layers;             // For array textures
    int mipmapLevels;       // 0 = auto-generate
    TextureWrap wrapS;
    TextureWrap wrapT;
    TextureWrap wrapR;      // For 3D/cube
    TextureFilter minFilter;
    TextureFilter magFilter;
    float anisotropy;
    bool generateMipmaps;
    bool immutable;         // Use glTexStorage
} TextureParams;

/**
 * Texture handle
 */
typedef struct Texture {
    GLuint id;
    TextureType type;
    TextureFormat format;
    int width;
    int height;
    int depth;
    int layers;
    int mipmapLevels;
    size_t memorySize;
    uint64_t lastUsed;
    uint32_t refCount;
    uint64_t hash;          // For caching
    bool resident;          // For bindless
} Texture;

/**
 * Async texture load request
 */
typedef struct AsyncTextureRequest {
    const void* data;
    size_t dataSize;
    TextureParams params;
    void (*callback)(Texture* texture, void* userData);
    void* userData;
    volatile bool completed;
    volatile bool cancelled;
    Texture* result;
} AsyncTextureRequest;

// ============================================================================
// Texture Manager Context
// ============================================================================

typedef struct TextureManagerContext {
    // Pool
    Texture* texturePool;
    int poolSize;
    int poolUsed;
    
    // Statistics
    size_t totalMemory;
    size_t peakMemory;
    uint32_t textureCount;
    uint32_t cacheHits;
    uint32_t cacheMisses;
    
    // Configuration
    int maxTextureSize;
    float defaultAnisotropy;
    bool useCompression;
    bool useAsyncLoading;
    
    // Async loading
    void* asyncQueue;
    void* asyncThread;
    
    bool initialized;
} TextureManagerContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize texture manager
 */
bool textureManagerInit(int poolSize, int maxTextureSize);

/**
 * Shutdown texture manager
 */
void textureManagerShutdown(void);

/**
 * Create texture with parameters
 */
Texture* textureCreate(const TextureParams* params);

/**
 * Create texture and upload data
 */
Texture* textureCreateWithData(const TextureParams* params, const void* data);

/**
 * Destroy texture
 */
void textureDestroy(Texture* texture);

/**
 * Bind texture to unit
 */
void textureBind(Texture* texture, int unit);

/**
 * Unbind texture from unit
 */
void textureUnbind(TextureType type, int unit);

/**
 * Upload texture data
 */
void textureUpload(Texture* texture, int level, int x, int y, 
                   int width, int height, const void* data);

/**
 * Upload subregion
 */
void textureUploadSub(Texture* texture, int level, 
                      int xoff, int yoff, int zoff,
                      int width, int height, int depth,
                      const void* data);

/**
 * Generate mipmaps
 */
void textureGenerateMipmaps(Texture* texture);

/**
 * Set texture parameters
 */
void textureSetFilter(Texture* texture, TextureFilter min, TextureFilter mag);
void textureSetWrap(Texture* texture, TextureWrap s, TextureWrap t, TextureWrap r);
void textureSetAnisotropy(Texture* texture, float anisotropy);

// ============================================================================
// Async Loading
// ============================================================================

/**
 * Request async texture load
 */
AsyncTextureRequest* textureLoadAsync(const void* data, size_t dataSize,
                                       const TextureParams* params,
                                       void (*callback)(Texture*, void*),
                                       void* userData);

/**
 * Cancel async request
 */
void textureLoadAsyncCancel(AsyncTextureRequest* request);

/**
 * Process completed async loads (call from main thread)
 */
void textureProcessAsyncLoads(void);

// ============================================================================
// Cache / Pool
// ============================================================================

/**
 * Get texture from cache by hash
 */
Texture* textureCacheGet(uint64_t hash);

/**
 * Add texture to cache
 */
void textureCacheAdd(Texture* texture, uint64_t hash);

/**
 * Clear texture cache
 */
void textureCacheClear(void);

/**
 * Trim texture memory
 */
void textureManagerTrim(size_t targetSize);

/**
 * Get memory usage
 */
size_t textureManagerGetMemoryUsage(void);

/**
 * Get statistics
 */
void textureManagerGetStats(uint32_t* count, size_t* memory, 
                            uint32_t* hits, uint32_t* misses);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get GL internal format for TextureFormat
 */
GLenum textureGetGLInternalFormat(TextureFormat format);

/**
 * Get GL format for TextureFormat
 */
GLenum textureGetGLFormat(TextureFormat format);

/**
 * Get GL type for TextureFormat
 */
GLenum textureGetGLType(TextureFormat format);

/**
 * Get bytes per pixel for format
 */
int textureGetBytesPerPixel(TextureFormat format);

/**
 * Calculate mipmap levels for size
 */
int textureCalculateMipmapLevels(int width, int height);

/**
 * Get default texture parameters
 */
TextureParams textureGetDefaultParams(void);

#ifdef __cplusplus
}
#endif

#endif // TEXTURE_MANAGER_H
