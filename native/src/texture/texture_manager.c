/**
 * Texture Manager - Implementation
 */

#include "texture_manager.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../core/gl_wrapper.h"

#include <string.h>
#include <math.h>
#include <pthread.h>

// ============================================================================
// Forward declarations
// ============================================================================

bool glExtensionSupported(const char* extension);

// ============================================================================
// GL Extension constants
// ============================================================================

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#endif

#ifndef GL_COMPRESSED_RGBA_ASTC_6x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#endif

#ifndef GL_COMPRESSED_RGBA_ASTC_8x8_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#endif

// ============================================================================
// Global State
// ============================================================================

static TextureManagerContext* g_texMgr = NULL;
static pthread_mutex_t g_texMutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// Format Conversion
// ============================================================================

GLenum textureGetGLInternalFormat(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_RGBA8:          return GL_RGBA8;
        case TEX_FORMAT_RGB8:           return GL_RGB8;
        case TEX_FORMAT_RGBA16F:        return GL_RGBA16F;
        case TEX_FORMAT_RGB16F:         return GL_RGB16F;
        case TEX_FORMAT_R8:             return GL_R8;
        case TEX_FORMAT_RG8:            return GL_RG8;
        case TEX_FORMAT_DEPTH24:        return GL_DEPTH_COMPONENT24;
        case TEX_FORMAT_DEPTH32F:       return GL_DEPTH_COMPONENT32F;
        case TEX_FORMAT_DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
        case TEX_FORMAT_ETC2_RGB:       return GL_COMPRESSED_RGB8_ETC2;
        case TEX_FORMAT_ETC2_RGBA:      return GL_COMPRESSED_RGBA8_ETC2_EAC;
        case TEX_FORMAT_ASTC_4x4:       return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        case TEX_FORMAT_ASTC_6x6:       return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
        case TEX_FORMAT_ASTC_8x8:       return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
        default:                        return GL_RGBA8;
    }
}

GLenum textureGetGLFormat(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_RGBA8:
        case TEX_FORMAT_RGBA16F:        return GL_RGBA;
        case TEX_FORMAT_RGB8:
        case TEX_FORMAT_RGB16F:         return GL_RGB;
        case TEX_FORMAT_R8:             return GL_RED;
        case TEX_FORMAT_RG8:            return GL_RG;
        case TEX_FORMAT_DEPTH24:
        case TEX_FORMAT_DEPTH32F:       return GL_DEPTH_COMPONENT;
        case TEX_FORMAT_DEPTH24_STENCIL8: return GL_DEPTH_STENCIL;
        default:                        return GL_RGBA;
    }
}

GLenum textureGetGLType(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_RGBA8:
        case TEX_FORMAT_RGB8:
        case TEX_FORMAT_R8:
        case TEX_FORMAT_RG8:            return GL_UNSIGNED_BYTE;
        case TEX_FORMAT_RGBA16F:
        case TEX_FORMAT_RGB16F:         return GL_HALF_FLOAT;
        case TEX_FORMAT_DEPTH24:        return GL_UNSIGNED_INT;
        case TEX_FORMAT_DEPTH32F:       return GL_FLOAT;
        case TEX_FORMAT_DEPTH24_STENCIL8: return GL_UNSIGNED_INT_24_8;
        default:                        return GL_UNSIGNED_BYTE;
    }
}

int textureGetBytesPerPixel(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_RGBA8:          return 4;
        case TEX_FORMAT_RGB8:           return 3;
        case TEX_FORMAT_RGBA16F:        return 8;
        case TEX_FORMAT_RGB16F:         return 6;
        case TEX_FORMAT_R8:             return 1;
        case TEX_FORMAT_RG8:            return 2;
        case TEX_FORMAT_DEPTH24:        return 3;
        case TEX_FORMAT_DEPTH32F:       return 4;
        case TEX_FORMAT_DEPTH24_STENCIL8: return 4;
        default:                        return 4;
    }
}

int textureCalculateMipmapLevels(int width, int height) {
    int maxDim = width > height ? width : height;
    return (int)floor(log2(maxDim)) + 1;
}

TextureParams textureGetDefaultParams(void) {
    TextureParams params = {
        .type = TEX_TYPE_2D,
        .format = TEX_FORMAT_RGBA8,
        .width = 1,
        .height = 1,
        .depth = 1,
        .layers = 1,
        .mipmapLevels = 1,
        .wrapS = TEX_WRAP_REPEAT,
        .wrapT = TEX_WRAP_REPEAT,
        .wrapR = TEX_WRAP_REPEAT,
        .minFilter = TEX_FILTER_LINEAR_MIPMAP_LINEAR,
        .magFilter = TEX_FILTER_LINEAR,
        .anisotropy = DEFAULT_ANISOTROPY,
        .generateMipmaps = true,
        .immutable = true
    };
    return params;
}

// ============================================================================
// Initialization
// ============================================================================

bool textureManagerInit(int poolSize, int maxTextureSize) {
    pthread_mutex_lock(&g_texMutex);
    
    if (g_texMgr) {
        velocityLogWarn("Texture manager already initialized");
        pthread_mutex_unlock(&g_texMutex);
        return true;
    }
    
    velocityLogInfo("Initializing texture manager (pool: %d, max size: %d)", 
                    poolSize, maxTextureSize);
    
    g_texMgr = (TextureManagerContext*)velocityCalloc(1, sizeof(TextureManagerContext));
    if (!g_texMgr) {
        velocityLogError("Failed to allocate texture manager");
        pthread_mutex_unlock(&g_texMutex);
        return false;
    }
    
    if (poolSize <= 0) poolSize = MAX_TEXTURE_POOL_SIZE;
    
    g_texMgr->texturePool = (Texture*)velocityCalloc(poolSize, sizeof(Texture));
    if (!g_texMgr->texturePool) {
        velocityLogError("Failed to allocate texture pool");
        velocityFree(g_texMgr);
        g_texMgr = NULL;
        pthread_mutex_unlock(&g_texMutex);
        return false;
    }
    
    g_texMgr->poolSize = poolSize;
    g_texMgr->maxTextureSize = maxTextureSize > 0 ? maxTextureSize : 4096;
    g_texMgr->defaultAnisotropy = DEFAULT_ANISOTROPY;
    g_texMgr->useCompression = true;
    g_texMgr->initialized = true;
    
    velocityLogInfo("Texture manager initialized");
    pthread_mutex_unlock(&g_texMutex);
    
    return true;
}

void textureManagerShutdown(void) {
    pthread_mutex_lock(&g_texMutex);
    
    if (!g_texMgr) {
        pthread_mutex_unlock(&g_texMutex);
        return;
    }
    
    velocityLogInfo("Shutting down texture manager");
    
    // Delete all textures
    for (int i = 0; i < g_texMgr->poolUsed; i++) {
        if (g_texMgr->texturePool[i].id != 0) {
            glDeleteTextures(1, &g_texMgr->texturePool[i].id);
        }
    }
    
    velocityFree(g_texMgr->texturePool);
    velocityFree(g_texMgr);
    g_texMgr = NULL;
    
    pthread_mutex_unlock(&g_texMutex);
}

// ============================================================================
// Texture Creation
// ============================================================================

static Texture* allocateTextureSlot(void) {
    for (int i = 0; i < g_texMgr->poolSize; i++) {
        if (g_texMgr->texturePool[i].id == 0) {
            if (i >= g_texMgr->poolUsed) {
                g_texMgr->poolUsed = i + 1;
            }
            return &g_texMgr->texturePool[i];
        }
    }
    
    velocityLogError("Texture pool exhausted!");
    return NULL;
}

Texture* textureCreate(const TextureParams* params) {
    if (!g_texMgr || !params) return NULL;
    
    pthread_mutex_lock(&g_texMutex);
    
    Texture* tex = allocateTextureSlot();
    if (!tex) {
        pthread_mutex_unlock(&g_texMutex);
        return NULL;
    }
    
    glGenTextures(1, &tex->id);
    if (tex->id == 0) {
        velocityLogError("Failed to generate texture");
        pthread_mutex_unlock(&g_texMutex);
        return NULL;
    }
    
    tex->type = params->type;
    tex->format = params->format;
    tex->width = params->width;
    tex->height = params->height;
    tex->depth = params->depth;
    tex->layers = params->layers;
    tex->refCount = 1;
    
    // Calculate mipmap levels
    if (params->mipmapLevels > 0) {
        tex->mipmapLevels = params->mipmapLevels;
    } else if (params->generateMipmaps) {
        tex->mipmapLevels = textureCalculateMipmapLevels(params->width, params->height);
    } else {
        tex->mipmapLevels = 1;
    }
    
    // Bind and configure
    glBindTexture(params->type, tex->id);
    
    GLenum internalFormat = textureGetGLInternalFormat(params->format);
    
    // Create storage
    if (params->immutable) {
        switch (params->type) {
            case TEX_TYPE_2D:
                glTexStorage2D(GL_TEXTURE_2D, tex->mipmapLevels, 
                              internalFormat, params->width, params->height);
                break;
            case TEX_TYPE_3D:
                glTexStorage3D(GL_TEXTURE_3D, tex->mipmapLevels,
                              internalFormat, params->width, params->height, params->depth);
                break;
            case TEX_TYPE_2D_ARRAY:
                glTexStorage3D(GL_TEXTURE_2D_ARRAY, tex->mipmapLevels,
                              internalFormat, params->width, params->height, params->layers);
                break;
            case TEX_TYPE_CUBE:
                glTexStorage2D(GL_TEXTURE_CUBE_MAP, tex->mipmapLevels,
                              internalFormat, params->width, params->height);
                break;
        }
    }
    
    // Set parameters
    glTexParameteri(params->type, GL_TEXTURE_MIN_FILTER, params->minFilter);
    glTexParameteri(params->type, GL_TEXTURE_MAG_FILTER, params->magFilter);
    glTexParameteri(params->type, GL_TEXTURE_WRAP_S, params->wrapS);
    glTexParameteri(params->type, GL_TEXTURE_WRAP_T, params->wrapT);
    
    if (params->type == TEX_TYPE_3D || params->type == TEX_TYPE_CUBE) {
        glTexParameteri(params->type, GL_TEXTURE_WRAP_R, params->wrapR);
    }
    
    // Anisotropic filtering
    if (params->anisotropy > 1.0f && glExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        glTexParameterf(params->type, GL_TEXTURE_MAX_ANISOTROPY_EXT, params->anisotropy);
    }
    
    // Calculate memory size
    tex->memorySize = params->width * params->height * textureGetBytesPerPixel(params->format);
    if (tex->mipmapLevels > 1) {
        tex->memorySize = (size_t)(tex->memorySize * 1.33f);  // Mipmap overhead
    }
    
    g_texMgr->totalMemory += tex->memorySize;
    g_texMgr->textureCount++;
    
    if (g_texMgr->totalMemory > g_texMgr->peakMemory) {
        g_texMgr->peakMemory = g_texMgr->totalMemory;
    }
    
    glBindTexture(params->type, 0);
    
    pthread_mutex_unlock(&g_texMutex);
    
    return tex;
}

Texture* textureCreateWithData(const TextureParams* params, const void* data) {
    Texture* tex = textureCreate(params);
    if (!tex || !data) return tex;
    
    textureUpload(tex, 0, 0, 0, params->width, params->height, data);
    
    if (params->generateMipmaps && tex->mipmapLevels > 1) {
        textureGenerateMipmaps(tex);
    }
    
    return tex;
}

void textureDestroy(Texture* texture) {
    if (!g_texMgr || !texture || texture->id == 0) return;
    
    pthread_mutex_lock(&g_texMutex);
    
    texture->refCount--;
    
    if (texture->refCount <= 0) {
        glDeleteTextures(1, &texture->id);
        
        g_texMgr->totalMemory -= texture->memorySize;
        g_texMgr->textureCount--;
        
        memset(texture, 0, sizeof(Texture));
    }
    
    pthread_mutex_unlock(&g_texMutex);
}

// ============================================================================
// Texture Operations
// ============================================================================

void textureBind(Texture* texture, int unit) {
    if (!texture || texture->id == 0) return;
    
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->type, texture->id);
    
    // Update usage time
    texture->lastUsed = 0; // Would use actual timestamp
}

void textureUnbind(TextureType type, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(type, 0);
}

void textureUpload(Texture* texture, int level, int x, int y,
                   int width, int height, const void* data) {
    if (!texture || texture->id == 0 || !data) return;
    
    GLenum format = textureGetGLFormat(texture->format);
    GLenum type = textureGetGLType(texture->format);
    
    glBindTexture(texture->type, texture->id);
    
    if (texture->type == TEX_TYPE_2D) {
        glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, format, type, data);
    }
    
    glBindTexture(texture->type, 0);
}

void textureUploadSub(Texture* texture, int level,
                      int xoff, int yoff, int zoff,
                      int width, int height, int depth,
                      const void* data) {
    if (!texture || texture->id == 0 || !data) return;
    
    GLenum format = textureGetGLFormat(texture->format);
    GLenum type = textureGetGLType(texture->format);
    
    glBindTexture(texture->type, texture->id);
    
    if (texture->type == TEX_TYPE_3D || texture->type == TEX_TYPE_2D_ARRAY) {
        glTexSubImage3D(texture->type, level, xoff, yoff, zoff,
                        width, height, depth, format, type, data);
    }
    
    glBindTexture(texture->type, 0);
}

void textureGenerateMipmaps(Texture* texture) {
    if (!texture || texture->id == 0) return;
    
    glBindTexture(texture->type, texture->id);
    glGenerateMipmap(texture->type);
    glBindTexture(texture->type, 0);
}

void textureSetFilter(Texture* texture, TextureFilter min, TextureFilter mag) {
    if (!texture || texture->id == 0) return;
    
    glBindTexture(texture->type, texture->id);
    glTexParameteri(texture->type, GL_TEXTURE_MIN_FILTER, min);
    glTexParameteri(texture->type, GL_TEXTURE_MAG_FILTER, mag);
    glBindTexture(texture->type, 0);
}

void textureSetWrap(Texture* texture, TextureWrap s, TextureWrap t, TextureWrap r) {
    if (!texture || texture->id == 0) return;
    
    glBindTexture(texture->type, texture->id);
    glTexParameteri(texture->type, GL_TEXTURE_WRAP_S, s);
    glTexParameteri(texture->type, GL_TEXTURE_WRAP_T, t);
    if (texture->type == TEX_TYPE_3D || texture->type == TEX_TYPE_CUBE) {
        glTexParameteri(texture->type, GL_TEXTURE_WRAP_R, r);
    }
    glBindTexture(texture->type, 0);
}

void textureSetAnisotropy(Texture* texture, float anisotropy) {
    if (!texture || texture->id == 0) return;
    
    if (glExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        glBindTexture(texture->type, texture->id);
        glTexParameterf(texture->type, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        glBindTexture(texture->type, 0);
    }
}

// ============================================================================
// Statistics
// ============================================================================

size_t textureManagerGetMemoryUsage(void) {
    if (!g_texMgr) return 0;
    return g_texMgr->totalMemory;
}

void textureManagerGetStats(uint32_t* count, size_t* memory,
                            uint32_t* hits, uint32_t* misses) {
    if (!g_texMgr) {
        if (count) *count = 0;
        if (memory) *memory = 0;
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        return;
    }
    
    pthread_mutex_lock(&g_texMutex);
    if (count) *count = g_texMgr->textureCount;
    if (memory) *memory = g_texMgr->totalMemory;
    if (hits) *hits = g_texMgr->cacheHits;
    if (misses) *misses = g_texMgr->cacheMisses;
    pthread_mutex_unlock(&g_texMutex);
}

void textureManagerTrim(size_t targetSize) {
    if (!g_texMgr || g_texMgr->totalMemory <= targetSize) return;
    
    velocityLogInfo("Trimming texture memory from %zu to %zu", 
                    g_texMgr->totalMemory, targetSize);
    
    // TODO: Implement LRU eviction based on lastUsed timestamps
}
