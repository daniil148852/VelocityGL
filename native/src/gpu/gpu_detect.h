/**
 * GPU Detection and Capability Query
 */

#ifndef GPU_DETECT_H
#define GPU_DETECT_H

#include "velocity_gl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GPU Identification
// ============================================================================

/**
 * Adreno GPU generations
 */
typedef enum AdrenoGeneration {
    ADRENO_UNKNOWN = 0,
    ADRENO_5XX,      // Adreno 5xx (old)
    ADRENO_6XX,      // Adreno 6xx
    ADRENO_7XX       // Adreno 7xx (latest)
} AdrenoGeneration;

/**
 * Mali GPU generations
 */
typedef enum MaliGeneration {
    MALI_UNKNOWN = 0,
    MALI_MIDGARD,    // Mali-T xxx
    MALI_BIFROST,    // Mali-G71, G72, G76
    MALI_VALHALL,    // Mali-G77, G78, G710
    MALI_5TH_GEN     // Mali-G720, Immortalis
} MaliGeneration;

/**
 * Detailed GPU info
 */
typedef struct GPUInfo {
    VelocityGPUVendor vendor;
    
    union {
        AdrenoGeneration adreno;
        MaliGeneration mali;
        int generic;
    } generation;
    
    int modelNumber;           // e.g., 730 for Adreno 730
    int coreCount;             // If known
    int maxClockMHz;           // If known
    
    // Performance tier (1-5, 5 is best)
    int performanceTier;
    
    // Feature support
    bool supportsASTCHDR;
    bool supportsETC2;
    bool supportsFP16;
    bool supportsInt16;
    bool hasProgramBinarySupport;
    int numBinaryFormats;
    
} GPUInfo;

// ============================================================================
// Detection Functions
// ============================================================================

/**
 * Detect GPU and fill capabilities
 */
void gpuDetect(VelocityGPUCaps* caps);

/**
 * Get detailed GPU info
 */
GPUInfo gpuGetInfo(void);

/**
 * Apply GPU-specific optimizations
 */
void gpuApplyTweaks(VelocityGPUVendor vendor);

/**
 * Check if specific extension is available
 */
bool gpuHasExtension(const char* extension);

/**
 * Get recommended settings for this GPU
 */
void gpuGetRecommendedSettings(VelocityConfig* config);

// ============================================================================
// Vendor-Specific Tweaks
// ============================================================================

/**
 * Apply Adreno-specific optimizations
 */
void gpuApplyAdrenoTweaks(AdrenoGeneration gen, int model);

/**
 * Apply Mali-specific optimizations  
 */
void gpuApplyMaliTweaks(MaliGeneration gen, int model);

/**
 * Apply PowerVR-specific optimizations
 */
void gpuApplyPowerVRTweaks(int model);

#ifdef __cplusplus
}
#endif

#endif // GPU_DETECT_H
