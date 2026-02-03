/**
 * VelocityGL - Main Entry Point
 * Library initialization and public API implementation
 */

#include "velocity_gl.h"
#include "core/gl_wrapper.h"
#include "shader/shader_cache.h"
#include "texture/texture_manager.h"
#include "buffer/buffer_pool.h"
#include "buffer/draw_batcher.h"
#include "optimize/resolution_scaler.h"
#include "gpu/gpu_detect.h"
#include "gl/gl_functions.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

// ============================================================================
// Version Info
// ============================================================================

static const char* VELOCITY_BUILD_DATE = __DATE__ " " __TIME__;

// ============================================================================
// Default Configuration
// ============================================================================

VelocityConfig velocityGetDefaultConfig(void) {
    VelocityConfig config = {
        // General
        .quality = VELOCITY_QUALITY_MEDIUM,
        .backend = VELOCITY_BACKEND_GLES3,
        
        // Shader caching
        .shaderCache = VELOCITY_CACHE_DISK,
        .shaderCachePath = "/sdcard/VelocityGL/cache",
        .shaderCacheMaxSize = 64 * 1024 * 1024,  // 64 MB
        
        // Resolution scaling
        .enableDynamicResolution = true,
        .minResolutionScale = 0.5f,
        .maxResolutionScale = 1.0f,
        .targetFPS = 60,
        
        // Draw call optimization
        .enableDrawBatching = true,
        .enableInstancing = true,
        .maxBatchSize = 128,
        
        // Texture optimization
        .enableTextureCompression = true,
        .enableAsyncTextureLoad = true,
        .texturePoolSize = 128,  // MB
        .maxTextureSize = 4096,
        
        // Buffer optimization
        .enableBufferPooling = true,
        .bufferPoolSize = 32,    // MB
        .enablePersistentMapping = true,
        
        // GPU specific
        .enableGPUSpecificTweaks = true,
        .forceCompatibilityMode = false,
        
        // Debug
        .enableDebugOutput = false,
        .enableProfiling = true,
        .logPath = NULL
    };
    
    return config;
}

// ============================================================================
// Initialization
// ============================================================================

VELOCITY_API bool velocityInit(const VelocityConfig* config) {
    VelocityConfig cfg;
    
    if (config) {
        memcpy(&cfg, config, sizeof(VelocityConfig));
    } else {
        cfg = velocityGetDefaultConfig();
    }
    
    // Initialize logging
    VelocityLogLevel logLevel = cfg.enableDebugOutput ? VELOCITY_LOG_DEBUG : VELOCITY_LOG_INFO;
    velocityLogInit(cfg.logPath, logLevel);
    
    velocityLogInfo("========================================");
    velocityLogInfo("VelocityGL v%s", VELOCITY_VERSION_STRING);
    velocityLogInfo("Build: %s", VELOCITY_BUILD_DATE);
    velocityLogInfo("========================================");
    
    // Initialize memory system
    velocityMemoryInit();
    
    // Initialize core wrapper
    if (!glWrapperInit(&cfg)) {
        velocityLogError("Failed to initialize GL wrapper");
        return false;
    }
    
    // Initialize GL function table
    if (!glFunctionsInit()) {
        velocityLogError("Failed to initialize GL functions");
        glWrapperShutdown();
        return false;
    }
    
    velocityLogInfo("VelocityGL initialized successfully");
    velocityLogInfo("  Quality: %d", cfg.quality);
    velocityLogInfo("  Shader Cache: %s", 
                    cfg.shaderCache == VELOCITY_CACHE_DISABLED ? "disabled" :
                    cfg.shaderCache == VELOCITY_CACHE_MEMORY_ONLY ? "memory" : "disk");
    velocityLogInfo("  Dynamic Resolution: %s", cfg.enableDynamicResolution ? "yes" : "no");
    velocityLogInfo("  Draw Batching: %s", cfg.enableDrawBatching ? "yes" : "no");
    
    return true;
}

VELOCITY_API bool velocityInitDefault(void) {
    return velocityInit(NULL);
}

VELOCITY_API void velocityShutdown(void) {
    velocityLogInfo("Shutting down VelocityGL...");
    
    // Shutdown subsystems in reverse order
    resolutionScalerShutdown();
    drawBatcherShutdown();
    bufferManagerShutdown();
    textureManagerShutdown();
    glFunctionsShutdown();
    glWrapperShutdown();
    
    // Check for memory leaks
    velocityMemoryCheckLeaks();
    velocityMemoryShutdown();
    
    velocityLogInfo("VelocityGL shutdown complete");
    velocityLogShutdown();
}

VELOCITY_API bool velocityUpdateConfig(const VelocityConfig* config) {
    if (!config || !g_wrapperCtx) {
        return false;
    }
    
    velocityLogInfo("Updating configuration...");
    
    // Update stored config
    memcpy(&g_wrapperCtx->config, config, sizeof(VelocityConfig));
    
    // Apply GPU-recommended settings if enabled
    if (config->enableGPUSpecificTweaks) {
        gpuGetRecommendedSettings(&g_wrapperCtx->config);
    }
    
    // Update resolution scaler
    if (resolutionScalerIsEnabled() != config->enableDynamicResolution) {
        resolutionScalerSetEnabled(config->enableDynamicResolution);
    }
    
    // Update draw batcher
    drawBatcherSetEnabled(config->enableDrawBatching);
    drawBatcherSetInstancing(config->enableInstancing);
    
    return true;
}

VELOCITY_API VelocityConfig velocityGetConfig(void) {
    if (g_wrapperCtx) {
        return g_wrapperCtx->config;
    }
    return velocityGetDefaultConfig();
}

// ============================================================================
// Context Management
// ============================================================================

VELOCITY_API bool velocityCreateContext(void* nativeWindow, void* eglDisplay) {
    if (!g_wrapperCtx) {
        velocityLogError("VelocityGL not initialized");
        return false;
    }
    
    velocityLogInfo("Creating rendering context...");
    
    // Create GL context
    if (!glWrapperCreateContext(nativeWindow, (EGLDisplay)eglDisplay)) {
        velocityLogError("Failed to create GL context");
        return false;
    }
    
    // Initialize subsystems that need GL context
    
    // Texture manager
    if (!textureManagerInit(g_wrapperCtx->config.texturePoolSize, 
                            g_wrapperCtx->config.maxTextureSize)) {
        velocityLogWarn("Texture manager initialization failed");
    }
    
    // Buffer manager
    if (!bufferManagerInit(g_wrapperCtx->config.bufferPoolSize * 1024 * 1024)) {
        velocityLogWarn("Buffer manager initialization failed");
    }
    
    // Draw batcher
    if (!drawBatcherInit(g_wrapperCtx->config.maxBatchSize * 8)) {
        velocityLogWarn("Draw batcher initialization failed");
    }
    
    // Resolution scaler
    if (g_wrapperCtx->config.enableDynamicResolution) {
        ScalerConfig scalerCfg = {
            .enabled = true,
            .minScale = g_wrapperCtx->config.minResolutionScale,
            .maxScale = g_wrapperCtx->config.maxResolutionScale,
            .targetFPS = g_wrapperCtx->config.targetFPS,
            .adjustSpeed = 0.1f,
            .upscaleMethod = UPSCALE_BILINEAR,
            .sharpening = true,
            .sharpenAmount = 0.3f
        };
        
        if (!resolutionScalerInit(g_wrapperCtx->windowWidth, 
                                   g_wrapperCtx->windowHeight, 
                                   &scalerCfg)) {
            velocityLogWarn("Resolution scaler initialization failed");
        }
    }
    
    velocityLogInfo("Rendering context created successfully");
    velocityLogInfo("  Window: %dx%d", g_wrapperCtx->windowWidth, g_wrapperCtx->windowHeight);
    
    return true;
}

VELOCITY_API void velocityDestroyContext(void) {
    if (!g_wrapperCtx) return;
    
    velocityLogInfo("Destroying rendering context...");
    
    resolutionScalerShutdown();
    drawBatcherShutdown();
    bufferManagerShutdown();
    textureManagerShutdown();
    
    glWrapperDestroyContext();
}

VELOCITY_API void velocitySwapBuffers(void) {
    if (!g_wrapperCtx) return;
    
    // End resolution scaler pass
    resolutionScalerEndFrame();
    
    // Swap buffers
    glWrapperSwapBuffers();
}

VELOCITY_API bool velocityMakeCurrent(void) {
    return glWrapperMakeCurrent();
}

// ============================================================================
// Frame Management
// ============================================================================

VELOCITY_API void velocityBeginFrame(void) {
    if (!g_wrapperCtx) return;
    
    glWrapperBeginFrame();
    bufferStreamBeginFrame();
    drawBatcherBeginFrame();
    
    // Begin resolution scaler
    int renderWidth, renderHeight;
    resolutionScalerBeginFrame(&renderWidth, &renderHeight);
    
    // Update stats
    g_wrapperCtx->stats.renderWidth = renderWidth;
    g_wrapperCtx->stats.renderHeight = renderHeight;
    g_wrapperCtx->stats.currentResolutionScale = resolutionScalerGetScale();
}

VELOCITY_API void velocityEndFrame(void) {
    if (!g_wrapperCtx) return;
    
    // Flush draw batcher
    drawBatcherEndFrame();
    
    // End streaming buffer frame
    bufferStreamEndFrame();
    
    // Record frame timing
    glWrapperEndFrame();
    
    // Update resolution scaler with frame time
    resolutionScalerRecordFrameTime(g_wrapperCtx->stats.frameTimeMs);
}

// ============================================================================
// Statistics
// ============================================================================

VELOCITY_API VelocityStats velocityGetStats(void) {
    VelocityStats stats = {0};
    
    if (g_wrapperCtx) {
        memcpy(&stats, &g_wrapperCtx->stats, sizeof(VelocityStats));
        
        // Add shader cache stats
        shaderCacheGetStats(&stats.shaderCacheHits, 
                            &stats.shaderCacheMisses, 
                            &stats.shaderCacheSize);
        
        // Add texture memory
        stats.textureMemory = textureManagerGetMemoryUsage();
        
        // Add buffer memory
        size_t bufAlloc, bufUsed;
        uint32_t allocCount;
        bufferManagerGetStats(&bufAlloc, &bufUsed, &allocCount);
        stats.bufferMemory = bufUsed;
    }
    
    return stats;
}

VELOCITY_API void velocityResetStats(void) {
    if (g_wrapperCtx) {
        memset(&g_wrapperCtx->stats, 0, sizeof(VelocityStats));
    }
    drawBatcherResetStats();
}

VELOCITY_API VelocityGPUCaps velocityGetGPUCaps(void) {
    VelocityGPUCaps caps = {0};
    
    if (g_wrapperCtx) {
        memcpy(&caps, &g_wrapperCtx->gpuCaps, sizeof(VelocityGPUCaps));
    }
    
    return caps;
}

// ============================================================================
// Shader Cache
// ============================================================================

VELOCITY_API void velocityPreloadShaders(void) {
    velocityLogInfo("Preloading common shaders...");
    shaderCachePreload();
}

VELOCITY_API void velocityClearShaderCache(void) {
    velocityLogInfo("Clearing shader cache...");
    shaderCacheClear();
}

VELOCITY_API size_t velocityGetShaderCacheSize(void) {
    uint32_t hits, misses;
    size_t size;
    shaderCacheGetStats(&hits, &misses, &size);
    return size;
}

VELOCITY_API void velocityFlushShaderCache(void) {
    shaderCacheFlush();
}

// ============================================================================
// Dynamic Resolution
// ============================================================================

VELOCITY_API void velocitySetResolutionScale(float scale) {
    resolutionScalerSetScale(scale);
}

VELOCITY_API float velocityGetResolutionScale(void) {
    return resolutionScalerGetScale();
}

VELOCITY_API void velocitySetDynamicResolution(bool enabled) {
    resolutionScalerSetEnabled(enabled);
}

// ============================================================================
// Memory Management
// ============================================================================

VELOCITY_API void velocityTrimMemory(int level) {
    velocityLogInfo("Trimming memory (level: %d)...", level);
    
    // Different levels of trimming
    switch (level) {
        case 0:  // Light trim
            bufferManagerTrim();
            break;
            
        case 1:  // Medium trim
            bufferManagerTrim();
            textureManagerTrim(textureManagerGetMemoryUsage() / 2);
            break;
            
        case 2:  // Heavy trim
            bufferManagerTrim();
            textureManagerTrim(textureManagerGetMemoryUsage() / 4);
            shaderCacheClear();
            break;
            
        default:  // Emergency trim
            bufferManagerTrim();
            textureCacheClear();
            shaderCacheClear();
            velocityMemoryTrim();
            break;
    }
}

VELOCITY_API size_t velocityGetMemoryUsage(void) {
    size_t total = 0;
    
    total += velocityMemoryGetUsage();
    total += textureManagerGetMemoryUsage();
    
    size_t bufAlloc, bufUsed;
    uint32_t allocCount;
    bufferManagerGetStats(&bufAlloc, &bufUsed, &allocCount);
    total += bufUsed;
    
    return total;
}

// ============================================================================
// Main Entry Point for Launchers
// ============================================================================

VELOCITY_API void* velocityGetProcAddress(const char* procName) {
    if (!procName) {
        return NULL;
    }
    
    // Check our wrapped functions first
    void* proc = glFunctionsGetProc(procName);
    
    if (proc) {
        return proc;
    }
    
    // Fall back to native EGL lookup
    return (void*)eglGetProcAddress(procName);
}

// ============================================================================
// Library Entry Points (for dlopen)
// ============================================================================

// These are the standard entry points that launchers look for

void* glXGetProcAddress(const char* procName) {
    return velocityGetProcAddress(procName);
}

void* glXGetProcAddressARB(const char* procName) {
    return velocityGetProcAddress(procName);
}

// OSMesa compatibility (for some launchers)
void* OSMesaGetProcAddress(const char* procName) {
    return velocityGetProcAddress(procName);
}

// EGL passthrough
EGLBoolean eglInitialize_velocity(EGLDisplay dpy, EGLint* major, EGLint* minor) {
    return eglInitialize(dpy, major, minor);
}

EGLBoolean eglTerminate_velocity(EGLDisplay dpy) {
    return eglTerminate(dpy);
}

// ============================================================================
// JNI Entry Points (for Android app integration)
// ============================================================================

#include <jni.h>

JNIEXPORT jboolean JNICALL
Java_com_velocitygl_VelocityGL_nativeInit(JNIEnv* env, jclass clazz, jstring configPath) {
    const char* path = NULL;
    if (configPath) {
        path = (*env)->GetStringUTFChars(env, configPath, NULL);
    }
    
    VelocityConfig config = velocityGetDefaultConfig();
    if (path) {
        config.shaderCachePath = path;
    }
    
    jboolean result = velocityInit(&config) ? JNI_TRUE : JNI_FALSE;
    
    if (path) {
        (*env)->ReleaseStringUTFChars(env, configPath, path);
    }
    
    return result;
}

JNIEXPORT void JNICALL
Java_com_velocitygl_VelocityGL_nativeShutdown(JNIEnv* env, jclass clazz) {
    velocityShutdown();
}

JNIEXPORT jboolean JNICALL
Java_com_velocitygl_VelocityGL_nativeCreateContext(JNIEnv* env, jclass clazz, 
                                                    jobject surface, jlong eglDisplay) {
    // Get native window from Surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        return JNI_FALSE;
    }
    
    jboolean result = velocityCreateContext(window, (void*)eglDisplay) ? JNI_TRUE : JNI_FALSE;
    
    return result;
}

JNIEXPORT void JNICALL
Java_com_velocitygl_VelocityGL_nativeDestroyContext(JNIEnv* env, jclass clazz) {
    velocityDestroyContext();
}

JNIEXPORT void JNICALL
Java_com_velocitygl_VelocityGL_nativeSwapBuffers(JNIEnv* env, jclass clazz) {
    velocitySwapBuffers();
}

JNIEXPORT jlong JNICALL
Java_com_velocitygl_VelocityGL_nativeGetProcAddress(JNIEnv* env, jclass clazz, jstring name) {
    const char* procName = (*env)->GetStringUTFChars(env, name, NULL);
    void* proc = velocityGetProcAddress(procName);
    (*env)->ReleaseStringUTFChars(env, name, procName);
    return (jlong)proc;
}

JNIEXPORT jfloat JNICALL
Java_com_velocitygl_VelocityGL_nativeGetFPS(JNIEnv* env, jclass clazz) {
    VelocityStats stats = velocityGetStats();
    return stats.currentFPS;
}

JNIEXPORT void JNICALL
Java_com_velocitygl_VelocityGL_nativeSetResolutionScale(JNIEnv* env, jclass clazz, jfloat scale) {
    velocitySetResolutionScale(scale);
}
