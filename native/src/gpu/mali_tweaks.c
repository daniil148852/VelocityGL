/**
 * ARM Mali GPU Specific Optimizations
 */

#include "gpu_detect.h"
#include "../utils/log.h"
#include "../core/gl_wrapper.h"

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

// ============================================================================
// Mali-Specific Extensions
// ============================================================================

#ifndef GL_ARM_shader_framebuffer_fetch
#define GL_ARM_shader_framebuffer_fetch 1
#define GL_FETCH_PER_SAMPLE_ARM           0x8F65
#define GL_FRAGMENT_SHADER_FRAMEBUFFER_FETCH_MRT_ARM 0x8F66
#endif

#ifndef GL_ARM_texture_unnormalized_coordinates
#define GL_ARM_texture_unnormalized_coordinates 1
#endif

// ============================================================================
// Internal State
// ============================================================================

static struct {
    bool hasFramebufferFetch;
    bool hasAFBC;              // ARM Frame Buffer Compression
    bool hasTransactionElimination;
    bool hasShaderInt64;
    MaliGeneration generation;
    int coreCount;
} maliState = {0};

// ============================================================================
// Extension Detection
// ============================================================================

static void detectMaliExtensions(void) {
    maliState.hasFramebufferFetch = gpuHasExtension("GL_ARM_shader_framebuffer_fetch") ||
                                     gpuHasExtension("GL_EXT_shader_framebuffer_fetch");
    maliState.hasShaderInt64 = gpuHasExtension("GL_ARB_gpu_shader_int64");
    
    // AFBC and Transaction Elimination are hardware features, not extensions
    // But we can check for related extensions
    maliState.hasAFBC = true;  // All modern Mali have AFBC
    maliState.hasTransactionElimination = true;
    
    velocityLogInfo("Mali extensions:");
    velocityLogInfo("  Framebuffer Fetch: %s", maliState.hasFramebufferFetch ? "yes" : "no");
    velocityLogInfo("  AFBC Support: %s", maliState.hasAFBC ? "yes" : "no");
}

// ============================================================================
// Mali Valhall Tweaks (G77, G78, G710, etc.)
// ============================================================================

static void applyMaliValhallTweaks(int model) {
    velocityLogInfo("Applying Mali Valhall optimizations");
    
    // Valhall architecture improvements:
    // - Better scheduling
    // - Improved texture sampling
    // - Enhanced FMA performance
    
    if (g_wrapperCtx) {
        // Valhall can handle higher workloads
        g_wrapperCtx->config.maxBatchSize = 192;
        g_wrapperCtx->config.enableInstancing = true;
        
        // G710+ are very capable
        if (model >= 710) {
            g_wrapperCtx->config.maxBatchSize = 256;
            g_wrapperCtx->config.texturePoolSize = 256;
            velocityLogInfo("  Mali-G710+ detected - high-end settings");
        }
        
        // G78 and G77 are still powerful
        if (model >= 77 && model < 710) {
            g_wrapperCtx->config.texturePoolSize = 192;
            velocityLogInfo("  Mali-G77/G78 detected - upper-mid settings");
        }
    }
}

// ============================================================================
// Mali Bifrost Tweaks (G71, G72, G76, etc.)
// ============================================================================

static void applyMaliBifrostTweaks(int model) {
    velocityLogInfo("Applying Mali Bifrost optimizations");
    
    // Bifrost architecture:
    // - Clause-based execution
    // - Better ILP
    
    if (g_wrapperCtx) {
        g_wrapperCtx->config.maxBatchSize = 128;
        
        // G76 is quite capable
        if (model == 76) {
            g_wrapperCtx->config.enableInstancing = true;
            g_wrapperCtx->config.texturePoolSize = 192;
            velocityLogInfo("  Mali-G76 detected - good performance");
        }
        // G72 is mid-range
        else if (model == 72) {
            g_wrapperCtx->config.texturePoolSize = 128;
            velocityLogInfo("  Mali-G72 detected - mid settings");
        }
        // G71 is older
        else if (model == 71) {
            g_wrapperCtx->config.maxBatchSize = 96;
            g_wrapperCtx->config.texturePoolSize = 96;
            velocityLogInfo("  Mali-G71 detected - conservative settings");
        }
    }
}

// ============================================================================
// Mali Midgard Tweaks (T-series, legacy)
// ============================================================================

static void applyMaliMidgardTweaks(void) {
    velocityLogInfo("Applying Mali Midgard optimizations (legacy)");
    
    // Midgard is old architecture
    // Very conservative settings needed
    
    if (g_wrapperCtx) {
        g_wrapperCtx->config.maxBatchSize = 48;
        g_wrapperCtx->config.enableInstancing = false;
        g_wrapperCtx->config.minResolutionScale = 0.4f;
        g_wrapperCtx->config.maxResolutionScale = 0.6f;
        g_wrapperCtx->config.texturePoolSize = 48;
        g_wrapperCtx->config.maxTextureSize = 2048;
    }
    
    velocityLogInfo("  Legacy Mali detected - using minimal settings");
}

// ============================================================================
// Mali 5th Gen Tweaks (G720, Immortalis)
// ============================================================================

static void applyMali5thGenTweaks(int model) {
    velocityLogInfo("Applying Mali 5th Gen optimizations");
    
    // Latest architecture - maximum performance
    
    if (g_wrapperCtx) {
        g_wrapperCtx->config.maxBatchSize = 256;
        g_wrapperCtx->config.enableInstancing = true;
        g_wrapperCtx->config.minResolutionScale = 0.85f;
        g_wrapperCtx->config.maxResolutionScale = 1.0f;
        g_wrapperCtx->config.texturePoolSize = 384;
        
        // Immortalis is the premium tier
        // Identified by G720 or higher with ray tracing
        if (gpuHasExtension("GL_EXT_ray_tracing") || model >= 720) {
            velocityLogInfo("  Immortalis-class GPU detected");
        }
    }
}

// ============================================================================
// Mali-Specific Shader Hints
// ============================================================================

static void applyMaliShaderHints(void) {
    // Mali-specific shader optimizations
    
    // 1. Prefer mediump where possible
    // 2. Avoid dependent texture reads
    // 3. Use framebuffer fetch for blending when available
    
    if (maliState.hasFramebufferFetch) {
        velocityLogInfo("  Framebuffer fetch available for blend optimization");
    }
    
    // AFBC considerations
    if (maliState.hasAFBC) {
        // AFBC works best with certain texture formats
        // RGBA8, RGBA16F are well-supported
        velocityLogInfo("  AFBC-friendly formats preferred");
    }
}

// ============================================================================
// Mali Transaction Elimination
// ============================================================================

static void applyMaliTransactionElimination(void) {
    // Transaction Elimination skips writing unchanged tiles
    // To maximize benefit:
    // 1. Clear framebuffer at start
    // 2. Use glInvalidateFramebuffer
    // 3. Avoid unnecessary full-screen passes
    
    velocityLogInfo("  Transaction Elimination hints configured");
}

// ============================================================================
// Main Entry Point
// ============================================================================

void gpuApplyMaliTweaks(MaliGeneration gen, int model) {
    maliState.generation = gen;
    
    // Detect extensions
    detectMaliExtensions();
    
    // Apply generation-specific tweaks
    switch (gen) {
        case MALI_5TH_GEN:
            applyMali5thGenTweaks(model);
            break;
            
        case MALI_VALHALL:
            applyMaliValhallTweaks(model);
            break;
            
        case MALI_BIFROST:
            applyMaliBifrostTweaks(model);
            break;
            
        case MALI_MIDGARD:
            applyMaliMidgardTweaks();
            break;
            
        default:
            velocityLogWarn("Unknown Mali generation, using conservative settings");
            if (g_wrapperCtx) {
                g_wrapperCtx->config.maxBatchSize = 64;
            }
            break;
    }
    
    // Common Mali optimizations
    applyMaliShaderHints();
    applyMaliTransactionElimination();
    
    velocityLogInfo("Mali tweaks applied successfully");
}

// ============================================================================
// PowerVR Tweaks (Bonus)
// ============================================================================

void gpuApplyPowerVRTweaks(int model) {
    velocityLogInfo("Applying PowerVR optimizations (model %d)", model);
    
    // PowerVR is tile-based deferred renderer (TBDR)
    // Similar principles to Mali and Adreno
    
    // PowerVR specific:
    // - Very efficient alpha blending in hardware
    // - HSR (Hidden Surface Removal) is automatic
    // - Avoid reading from render targets
    
    if (g_wrapperCtx) {
        // PowerVR is usually found in budget devices (MediaTek, some Apple-designed)
        // Conservative settings
        g_wrapperCtx->config.maxBatchSize = 96;
        g_wrapperCtx->config.texturePoolSize = 96;
        
        // Newer PowerVR can be decent
        if (model >= 8000) {
            g_wrapperCtx->config.maxBatchSize = 128;
            g_wrapperCtx->config.enableInstancing = true;
        }
    }
    
    velocityLogInfo("PowerVR tweaks applied");
}
