/**
 * VelocityGL - High Performance OpenGL Wrapper
 * Main public header
 */

#ifndef VELOCITY_GL_H
#define VELOCITY_GL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Version Info
// ============================================================================
#define VELOCITY_VERSION_MAJOR 1
#define VELOCITY_VERSION_MINOR 0
#define VELOCITY_VERSION_PATCH 0
#define VELOCITY_VERSION_STRING "1.0.0"

// ============================================================================
// Export Macros
// ============================================================================
#ifdef _WIN32
    #define VELOCITY_API __declspec(dllexport)
#else
    #define VELOCITY_API __attribute__((visibility("default")))
#endif

#define VELOCITY_CALL

// ============================================================================
// Configuration Structures
// ============================================================================

/**
 * GPU Vendor types
 */
typedef enum VelocityGPUVendor {
    VELOCITY_GPU_UNKNOWN = 0,
    VELOCITY_GPU_QUALCOMM_ADRENO,
    VELOCITY_GPU_ARM_MALI,
    VELOCITY_GPU_IMAGINATION_POWERVR,
    VELOCITY_GPU_SAMSUNG_XCLIPSE,
    VELOCITY_GPU_NVIDIA,
    VELOCITY_GPU_INTEL
} VelocityGPUVendor;

/**
 * Quality presets
 */
typedef enum VelocityQualityPreset {
    VELOCITY_QUALITY_ULTRA_LOW = 0,  // Maximum FPS, lowest quality
    VELOCITY_QUALITY_LOW,
    VELOCITY_QUALITY_MEDIUM,
    VELOCITY_QUALITY_HIGH,
    VELOCITY_QUALITY_ULTRA,          // Best quality
    VELOCITY_QUALITY_CUSTOM
} VelocityQualityPreset;

/**
 * Render backend
 */
typedef enum VelocityBackend {
    VELOCITY_BACKEND_GLES3 = 0,      // Default OpenGL ES 3.x
    VELOCITY_BACKEND_ANGLE_VULKAN,   // ANGLE with Vulkan
    VELOCITY_BACKEND_ZINK            // Zink (Mesa)
} VelocityBackend;

/**
 * Shader cache mode
 */
typedef enum VelocityShaderCacheMode {
    VELOCITY_CACHE_DISABLED = 0,
    VELOCITY_CACHE_MEMORY_ONLY,      // In-memory caching
    VELOCITY_CACHE_DISK,             // Persist to disk
    VELOCITY_CACHE_AGGRESSIVE        // Pre-compile common shaders
} VelocityShaderCacheMode;

/**
 * Main configuration
 */
typedef struct VelocityConfig {
    // General
    VelocityQualityPreset quality;
    VelocityBackend backend;
    
    // Shader caching
    VelocityShaderCacheMode shaderCache;
    const char* shaderCachePath;
    size_t shaderCacheMaxSize;       // Max cache size in bytes
    
    // Resolution scaling
    bool enableDynamicResolution;
    float minResolutionScale;        // e.g., 0.5 for 50%
    float maxResolutionScale;        // e.g., 1.0 for 100%
    int targetFPS;                   // Target for dynamic scaling
    
    // Draw call optimization
    bool enableDrawBatching;
    bool enableInstancing;
    int maxBatchSize;
    
    // Texture optimization
    bool enableTextureCompression;
    bool enableAsyncTextureLoad;
    int texturePoolSize;             // MB
    int maxTextureSize;              // Max dimension
    
    // Buffer optimization
    bool enableBufferPooling;
    int bufferPoolSize;              // MB
    bool enablePersistentMapping;
    
    // GPU specific
    bool enableGPUSpecificTweaks;
    bool forceCompatibilityMode;
    
    // Debug
    bool enableDebugOutput;
    bool enableProfiling;
    const char* logPath;
    
} VelocityConfig;

/**
 * Runtime statistics
 */
typedef struct VelocityStats {
    // Frame info
    float currentFPS;
    float avgFPS;
    float frameTimeMs;
    float gpuTimeMs;
    float cpuTimeMs;
    
    // Draw calls
    uint32_t drawCalls;
    uint32_t drawCallsSaved;         // Saved by batching
    uint32_t triangles;
    
    // Memory
    size_t textureMemory;
    size_t bufferMemory;
    size_t shaderCacheSize;
    
    // Shader cache
    uint32_t shaderCacheHits;
    uint32_t shaderCacheMisses;
    
    // Resolution
    float currentResolutionScale;
    int renderWidth;
    int renderHeight;
    
} VelocityStats;

/**
 * GPU capabilities
 */
typedef struct VelocityGPUCaps {
    VelocityGPUVendor vendor;
    char vendorString[64];
    char rendererString[128];
    char versionString[64];
    
    // OpenGL ES capabilities
    int glesVersionMajor;
    int glesVersionMinor;
    
    // Emulated OpenGL version
    int glVersionMajor;
    int glVersionMinor;
    
    // Limits
    int maxTextureSize;
    int maxTextureUnits;
    int maxVertexAttribs;
    int maxUniformBufferBindings;
    int maxShaderStorageBufferBindings;
    int maxComputeWorkGroupSize[3];
    
    // Extensions
    bool hasComputeShaders;
    bool hasGeometryShaders;
    bool hasTessellation;
    bool hasBindlessTextures;
    bool hasSparseTextures;
    bool hasShaderBinaryFormats;
    bool hasAnisotropicFiltering;
    float maxAnisotropy;
    
} VelocityGPUCaps;

// ============================================================================
// Core API Functions
// ============================================================================

/**
 * Initialize VelocityGL with configuration
 * Call once at startup before any GL calls
 */
VELOCITY_API bool velocityInit(const VelocityConfig* config);

/**
 * Initialize with default configuration
 */
VELOCITY_API bool velocityInitDefault(void);

/**
 * Shutdown VelocityGL
 * Call at application exit
 */
VELOCITY_API void velocityShutdown(void);

/**
 * Get default configuration
 */
VELOCITY_API VelocityConfig velocityGetDefaultConfig(void);

/**
 * Update configuration at runtime
 */
VELOCITY_API bool velocityUpdateConfig(const VelocityConfig* config);

/**
 * Get current configuration
 */
VELOCITY_API VelocityConfig velocityGetConfig(void);

// ============================================================================
// Context Management
// ============================================================================

/**
 * Create and make current a VelocityGL context
 */
VELOCITY_API bool velocityCreateContext(void* nativeWindow, void* eglDisplay);

/**
 * Destroy context
 */
VELOCITY_API void velocityDestroyContext(void);

/**
 * Swap buffers (end of frame)
 */
VELOCITY_API void velocitySwapBuffers(void);

/**
 * Make context current on this thread
 */
VELOCITY_API bool velocityMakeCurrent(void);

// ============================================================================
// Statistics & Debugging
// ============================================================================

/**
 * Get current statistics
 */
VELOCITY_API VelocityStats velocityGetStats(void);

/**
 * Reset statistics counters
 */
VELOCITY_API void velocityResetStats(void);

/**
 * Get GPU capabilities
 */
VELOCITY_API VelocityGPUCaps velocityGetGPUCaps(void);

/**
 * Start frame (call at beginning of each frame)
 */
VELOCITY_API void velocityBeginFrame(void);

/**
 * End frame (call at end of each frame)
 */
VELOCITY_API void velocityEndFrame(void);

// ============================================================================
// Shader Cache Control
// ============================================================================

/**
 * Preload common Minecraft shaders
 */
VELOCITY_API void velocityPreloadShaders(void);

/**
 * Clear shader cache
 */
VELOCITY_API void velocityClearShaderCache(void);

/**
 * Get shader cache statistics
 */
VELOCITY_API size_t velocityGetShaderCacheSize(void);

/**
 * Force compile/save shader cache to disk
 */
VELOCITY_API void velocityFlushShaderCache(void);

// ============================================================================
// Dynamic Resolution
// ============================================================================

/**
 * Set resolution scale manually (0.25 - 2.0)
 */
VELOCITY_API void velocitySetResolutionScale(float scale);

/**
 * Get current resolution scale
 */
VELOCITY_API float velocityGetResolutionScale(void);

/**
 * Enable/disable dynamic resolution
 */
VELOCITY_API void velocitySetDynamicResolution(bool enabled);

// ============================================================================
// Memory Management
// ============================================================================

/**
 * Trim memory usage (call during low memory situations)
 */
VELOCITY_API void velocityTrimMemory(int level);

/**
 * Get total memory usage
 */
VELOCITY_API size_t velocityGetMemoryUsage(void);

// ============================================================================
// GL Function Entry Point (for launcher integration)
// ============================================================================

/**
 * Get OpenGL function pointer
 * This is the main entry point used by launchers
 */
VELOCITY_API void* velocityGetProcAddress(const char* procName);

#ifdef __cplusplus
}
#endif

#endif // VELOCITY_GL_H
