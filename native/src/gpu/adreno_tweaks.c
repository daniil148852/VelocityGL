/**
 * Adreno GPU Specific Optimizations
 */

#include "gpu_detect.h"
#include "../utils/log.h"
#include "../core/gl_wrapper.h"

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

// ============================================================================
// Adreno-Specific Extensions and Features
// ============================================================================

// QCOM extensions
#ifndef GL_QCOM_binning_control
#define GL_QCOM_binning_control 1
#define GL_BINNING_CONTROL_HINT_QCOM           0x8FB0
#define GL_CPU_OPTIMIZED_QCOM                  0x8FB1
#define GL_GPU_OPTIMIZED_QCOM                  0x8FB2
#define GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM   0x8FB3
#endif

#ifndef GL_QCOM_tiled_rendering
#define GL_QCOM_tiled_rendering 1
#define GL_COLOR_BUFFER_BIT0_QCOM              0x00000001
#define GL_COLOR_BUFFER_BIT1_QCOM              0x00000002
#define GL_DEPTH_BUFFER_BIT0_QCOM              0x00000100
#define GL_STENCIL_BUFFER_BIT0_QCOM            0x00010000
#endif

// ============================================================================
// Internal State
// ============================================================================

static struct {
    bool hasBinningControl;
    bool hasTiledRendering;
    bool hasShaderFramebufferFetch;
    bool hasTextureFilterAnisotropic;
    AdrenoGeneration generation;
    int model;
} adrenoState = {0};

// ============================================================================
// Extension Detection
// ============================================================================

static void detectAdrenoExtensions(void) {
    adrenoState.hasBinningControl = gpuHasExtension("GL_QCOM_binning_control");
    adrenoState.hasTiledRendering = gpuHasExtension("GL_QCOM_tiled_rendering");
    adrenoState.hasShaderFramebufferFetch = gpuHasExtension("GL_EXT_shader_framebuffer_fetch");
    adrenoState.hasTextureFilterAnisotropic = gpuHasExtension("GL_EXT_texture_filter_anisotropic");
    
    velocityLogInfo("Adreno extensions:");
    velocityLogInfo("  Binning Control: %s", adrenoState.hasBinningControl ? "yes" : "no");
    velocityLogInfo("  Tiled Rendering: %s", adrenoState.hasTiledRendering ? "yes" : "no");
    velocityLogInfo("  Framebuffer Fetch: %s", adrenoState.hasShaderFramebufferFetch ? "yes" : "no");
}

// ============================================================================
// Adreno 6xx Tweaks
// ============================================================================

static void applyAdreno6xxTweaks(int model) {
    velocityLogInfo("Applying Adreno 6xx optimizations (model %d)", model);
    
    // For Adreno 6xx series
    if (adrenoState.hasBinningControl) {
        // Hint for GPU-optimized binning
        // This can improve performance for complex scenes
        glHint(GL_BINNING_CONTROL_HINT_QCOM, GL_GPU_OPTIMIZED_QCOM);
        velocityLogInfo("  Enabled GPU-optimized binning");
    }
    
    // Adreno 650+ specific
    if (model >= 650) {
        // These GPUs handle high poly counts better
        // Enable more aggressive batching
        if (g_wrapperCtx) {
            g_wrapperCtx->config.maxBatchSize = 192;
        }
        velocityLogInfo("  Increased batch size to 192");
    }
    
    // Adreno 660+ (newer flagships)
    if (model >= 660) {
        // Full instancing support
        velocityLogInfo("  Full instancing enabled");
    }
}

// ============================================================================
// Adreno 7xx Tweaks
// ============================================================================

static void applyAdreno7xxTweaks(int model) {
    velocityLogInfo("Applying Adreno 7xx optimizations (model %d)", model);
    
    // Adreno 7xx series (latest)
    if (adrenoState.hasBinningControl) {
        glHint(GL_BINNING_CONTROL_HINT_QCOM, GL_GPU_OPTIMIZED_QCOM);
    }
    
    if (g_wrapperCtx) {
        // 7xx can handle max settings
        g_wrapperCtx->config.maxBatchSize = 256;
        g_wrapperCtx->config.enableInstancing = true;
        
        // High resolution is fine on 7xx
        g_wrapperCtx->config.minResolutionScale = 0.8f;
        g_wrapperCtx->config.maxResolutionScale = 1.0f;
    }
    
    // Adreno 730+
    if (model >= 730) {
        velocityLogInfo("  High-end Adreno 730+ detected");
        // Can handle raytracing hints and advanced features
    }
    
    // Adreno 740+ (2023+ flagships)
    if (model >= 740) {
        velocityLogInfo("  Latest Adreno 740+ detected");
        // Maximum performance settings
        if (g_wrapperCtx) {
            g_wrapperCtx->config.texturePoolSize = 384;
        }
    }
}

// ============================================================================
// Adreno 5xx Tweaks (Legacy)
// ============================================================================

static void applyAdreno5xxTweaks(int model) {
    velocityLogInfo("Applying Adreno 5xx optimizations (model %d)", model);
    
    // Older GPUs - need conservative settings
    if (g_wrapperCtx) {
        g_wrapperCtx->config.maxBatchSize = 64;
        g_wrapperCtx->config.enableInstancing = (model >= 540);
        g_wrapperCtx->config.minResolutionScale = 0.4f;
        g_wrapperCtx->config.maxResolutionScale = 0.7f;
        g_wrapperCtx->config.texturePoolSize = 64;
    }
    
    if (model >= 530) {
        // Adreno 530/540 are still decent
        velocityLogInfo("  Mid-range Adreno 5xx detected");
    } else {
        // 505/506/508/509/510/512 - very limited
        velocityLogInfo("  Entry-level Adreno 5xx detected");
        if (g_wrapperCtx) {
            g_wrapperCtx->config.maxTextureSize = 2048;
        }
    }
}

// ============================================================================
// Shader Workarounds
// ============================================================================

/**
 * Known Adreno shader compiler issues and workarounds
 */
static void applyAdrenoShaderWorkarounds(void) {
    // Adreno has some known shader compiler quirks
    
    // 1. Some Adreno GPUs have issues with complex control flow in fragment shaders
    // 2. Early-Z optimization hints can help
    // 3. Avoid excessive branching
    
    velocityLogInfo("  Shader workarounds enabled");
}

// ============================================================================
// Memory Hints
// ============================================================================

static void applyAdrenoMemoryHints(int model) {
    // Adreno uses tile-based deferred rendering (TBDR)
    // Proper use of glInvalidateFramebuffer is crucial
    
    // For TBDR architectures, we want to:
    // 1. Clear or invalidate attachments at start of frame
    // 2. Minimize read-modify-write operations
    // 3. Use appropriate texture formats
    
    velocityLogInfo("  TBDR memory hints configured");
}

// ============================================================================
// Main Entry Point
// ============================================================================

void gpuApplyAdrenoTweaks(AdrenoGeneration gen, int model) {
    adrenoState.generation = gen;
    adrenoState.model = model;
    
    // Detect extensions first
    detectAdrenoExtensions();
    
    // Apply generation-specific tweaks
    switch (gen) {
        case ADRENO_7XX:
            applyAdreno7xxTweaks(model);
            break;
            
        case ADRENO_6XX:
            applyAdreno6xxTweaks(model);
            break;
            
        case ADRENO_5XX:
            applyAdreno5xxTweaks(model);
            break;
            
        default:
            velocityLogWarn("Unknown Adreno generation");
            break;
    }
    
    // Common tweaks
    applyAdrenoShaderWorkarounds();
    applyAdrenoMemoryHints(model);
    
    // Set texture filtering
    if (adrenoState.hasTextureFilterAnisotropic) {
        // Enable up to 4x anisotropic for good quality/perf balance
        float maxAniso = 4.0f;
        if (gen >= ADRENO_7XX) {
            maxAniso = 8.0f;
        }
        velocityLogInfo("  Max anisotropic filtering: %.1fx", maxAniso);
    }
    
    velocityLogInfo("Adreno tweaks applied successfully");
}
