/**
 * GPU Detection - Implementation
 */

#include "gpu_detect.h"
#include "../utils/log.h"
#include "../core/gl_wrapper.h"

#include <GLES3/gl32.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Extension Check
// ============================================================================

static char* g_extensionString = NULL;

bool gpuHasExtension(const char* extension) {
    if (!g_extensionString) {
        g_extensionString = (char*)glGetString(GL_EXTENSIONS);
    }
    
    if (!g_extensionString || !extension) {
        return false;
    }
    
    size_t extLen = strlen(extension);
    const char* ptr = g_extensionString;
    
    while ((ptr = strstr(ptr, extension)) != NULL) {
        // Check it's a complete word
        char before = (ptr == g_extensionString) ? ' ' : *(ptr - 1);
        char after = ptr[extLen];
        
        if ((before == ' ' || before == '\0') && 
            (after == ' ' || after == '\0')) {
            return true;
        }
        
        ptr += extLen;
    }
    
    return false;
}

// ============================================================================
// Vendor Detection
// ============================================================================

static VelocityGPUVendor detectVendor(const char* vendorStr, const char* rendererStr) {
    if (!vendorStr || !rendererStr) {
        return VELOCITY_GPU_UNKNOWN;
    }
    
    // Convert to lowercase for comparison
    char vendor[64], renderer[128];
    strncpy(vendor, vendorStr, sizeof(vendor) - 1);
    strncpy(renderer, rendererStr, sizeof(renderer) - 1);
    
    for (char* p = vendor; *p; p++) *p = tolower(*p);
    for (char* p = renderer; *p; p++) *p = tolower(*p);
    
    // Qualcomm Adreno
    if (strstr(vendor, "qualcomm") || strstr(renderer, "adreno")) {
        return VELOCITY_GPU_QUALCOMM_ADRENO;
    }
    
    // ARM Mali
    if (strstr(vendor, "arm") || strstr(renderer, "mali")) {
        return VELOCITY_GPU_ARM_MALI;
    }
    
    // Imagination PowerVR
    if (strstr(vendor, "imagination") || strstr(renderer, "powervr")) {
        return VELOCITY_GPU_IMAGINATION_POWERVR;
    }
    
    // Samsung Xclipse (AMD-based)
    if (strstr(renderer, "xclipse") || strstr(renderer, "samsung")) {
        return VELOCITY_GPU_SAMSUNG_XCLIPSE;
    }
    
    // NVIDIA
    if (strstr(vendor, "nvidia")) {
        return VELOCITY_GPU_NVIDIA;
    }
    
    // Intel
    if (strstr(vendor, "intel")) {
        return VELOCITY_GPU_INTEL;
    }
    
    return VELOCITY_GPU_UNKNOWN;
}

// ============================================================================
// Model Number Extraction
// ============================================================================

static int extractModelNumber(const char* renderer) {
    if (!renderer) return 0;
    
    // Find first digit sequence
    const char* p = renderer;
    while (*p) {
        if (isdigit(*p)) {
            return atoi(p);
        }
        p++;
    }
    
    return 0;
}

static AdrenoGeneration getAdrenoGeneration(int model) {
    if (model >= 700) return ADRENO_7XX;
    if (model >= 600) return ADRENO_6XX;
    if (model >= 500) return ADRENO_5XX;
    return ADRENO_UNKNOWN;
}

static MaliGeneration getMaliGeneration(const char* renderer) {
    if (strstr(renderer, "G7") && (strstr(renderer, "20") || strstr(renderer, "Immortalis"))) {
        return MALI_5TH_GEN;
    }
    if (strstr(renderer, "G77") || strstr(renderer, "G78") || strstr(renderer, "G710")) {
        return MALI_VALHALL;
    }
    if (strstr(renderer, "G71") || strstr(renderer, "G72") || strstr(renderer, "G76")) {
        return MALI_BIFROST;
    }
    if (strstr(renderer, "T")) {
        return MALI_MIDGARD;
    }
    return MALI_UNKNOWN;
}

static int calculatePerformanceTier(VelocityGPUVendor vendor, int model) {
    switch (vendor) {
        case VELOCITY_GPU_QUALCOMM_ADRENO:
            if (model >= 740) return 5;
            if (model >= 730) return 5;
            if (model >= 700) return 4;
            if (model >= 660) return 4;
            if (model >= 650) return 3;
            if (model >= 600) return 2;
            return 1;
            
        case VELOCITY_GPU_ARM_MALI:
            if (model >= 720) return 5;  // Immortalis-G720
            if (model >= 710) return 4;
            if (model >= 78) return 4;
            if (model >= 77) return 3;
            if (model >= 76) return 3;
            return 2;
            
        default:
            return 2;
    }
}

// ============================================================================
// Main Detection
// ============================================================================

void gpuDetect(VelocityGPUCaps* caps) {
    if (!caps) return;
    
    memset(caps, 0, sizeof(VelocityGPUCaps));
    
    // Get strings
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    
    if (vendor) strncpy(caps->vendorString, vendor, sizeof(caps->vendorString) - 1);
    if (renderer) strncpy(caps->rendererString, renderer, sizeof(caps->rendererString) - 1);
    if (version) strncpy(caps->versionString, version, sizeof(caps->versionString) - 1);
    
    // Detect vendor
    caps->vendor = detectVendor(vendor, renderer);
    
    // Parse GLES version
    if (version) {
        if (sscanf(version, "OpenGL ES %d.%d", &caps->glesVersionMajor, &caps->glesVersionMinor) != 2) {
            caps->glesVersionMajor = 3;
            caps->glesVersionMinor = 0;
        }
    }
    
    // Set emulated GL version based on ES version and features
    if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 2) {
        caps->glVersionMajor = 4;
        caps->glVersionMinor = 5;  // Can emulate GL 4.5
    } else if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1) {
        caps->glVersionMajor = 4;
        caps->glVersionMinor = 3;
    } else {
        caps->glVersionMajor = 3;
        caps->glVersionMinor = 3;
    }
    
    // Query limits
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->maxTextureSize);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &caps->maxTextureUnits);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps->maxVertexAttribs);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &caps->maxUniformBufferBindings);
    
    if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1) {
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &caps->maxShaderStorageBufferBindings);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &caps->maxComputeWorkGroupSize[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &caps->maxComputeWorkGroupSize[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &caps->maxComputeWorkGroupSize[2]);
    }
    
    // Check features
    caps->hasComputeShaders = (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1);
    caps->hasGeometryShaders = gpuHasExtension("GL_EXT_geometry_shader") || 
                               (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 2);
    caps->hasTessellation = gpuHasExtension("GL_EXT_tessellation_shader");
    caps->hasBindlessTextures = gpuHasExtension("GL_NV_bindless_texture") ||
                                 gpuHasExtension("GL_ARB_bindless_texture");
    
    // Anisotropic filtering
    caps->hasAnisotropicFiltering = gpuHasExtension("GL_EXT_texture_filter_anisotropic");
    if (caps->hasAnisotropicFiltering) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &caps->maxAnisotropy);
    }
    
    // Binary shader support
    GLint numFormats = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
    caps->hasShaderBinaryFormats = (numFormats > 0);
    
    velocityLogInfo("GPU Detection complete:");
    velocityLogInfo("  Vendor: %s", caps->vendorString);
    velocityLogInfo("  Renderer: %s", caps->rendererString);
    velocityLogInfo("  GLES: %d.%d", caps->glesVersionMajor, caps->glesVersionMinor);
    velocityLogInfo("  Emulating GL: %d.%d", caps->glVersionMajor, caps->glVersionMinor);
    velocityLogInfo("  Max Texture Size: %d", caps->maxTextureSize);
    velocityLogInfo("  Compute Shaders: %s", caps->hasComputeShaders ? "yes" : "no");
    velocityLogInfo("  Geometry Shaders: %s", caps->hasGeometryShaders ? "yes" : "no");
    velocityLogInfo("  Binary Shaders: %s (%d formats)", caps->hasShaderBinaryFormats ? "yes" : "no", numFormats);
}

GPUInfo gpuGetInfo(void) {
    GPUInfo info = {0};
    
    if (!g_wrapperCtx) return info;
    
    info.vendor = g_wrapperCtx->gpuCaps.vendor;
    info.modelNumber = extractModelNumber(g_wrapperCtx->gpuCaps.rendererString);
    
    switch (info.vendor) {
        case VELOCITY_GPU_QUALCOMM_ADRENO:
            info.generation.adreno = getAdrenoGeneration(info.modelNumber);
            break;
        case VELOCITY_GPU_ARM_MALI:
            info.generation.mali = getMaliGeneration(g_wrapperCtx->gpuCaps.rendererString);
            break;
        default:
            break;
    }
    
    info.performanceTier = calculatePerformanceTier(info.vendor, info.modelNumber);
    
    // Check compression support
    info.supportsETC2 = true;  // Mandatory in ES 3.0
    info.supportsASTCHDR = gpuHasExtension("GL_KHR_texture_compression_astc_hdr");
    info.supportsFP16 = gpuHasExtension("GL_EXT_shader_explicit_arithmetic_types_float16");
    info.hasProgramBinarySupport = g_wrapperCtx->gpuCaps.hasShaderBinaryFormats;
    
    return info;
}

// ============================================================================
// Apply Tweaks
// ============================================================================

void gpuApplyTweaks(VelocityGPUVendor vendor) {
    GPUInfo info = gpuGetInfo();
    
    velocityLogInfo("Applying GPU-specific tweaks for %s (model %d, tier %d)",
                    g_wrapperCtx->gpuCaps.vendorString, 
                    info.modelNumber, 
                    info.performanceTier);
    
    switch (vendor) {
        case VELOCITY_GPU_QUALCOMM_ADRENO:
            gpuApplyAdrenoTweaks(info.generation.adreno, info.modelNumber);
            break;
            
        case VELOCITY_GPU_ARM_MALI:
            gpuApplyMaliTweaks(info.generation.mali, info.modelNumber);
            break;
            
        case VELOCITY_GPU_IMAGINATION_POWERVR:
            gpuApplyPowerVRTweaks(info.modelNumber);
            break;
            
        default:
            velocityLogInfo("No specific tweaks for this GPU");
            break;
    }
}

void gpuGetRecommendedSettings(VelocityConfig* config) {
    if (!config) return;
    
    GPUInfo info = gpuGetInfo();
    
    // Base settings on performance tier
    switch (info.performanceTier) {
        case 5:  // High-end
            config->quality = VELOCITY_QUALITY_HIGH;
            config->minResolutionScale = 0.75f;
            config->maxResolutionScale = 1.0f;
            config->targetFPS = 60;
            config->maxBatchSize = 256;
            config->texturePoolSize = 256;
            config->enableInstancing = true;
            break;
            
        case 4:  // Upper mid
            config->quality = VELOCITY_QUALITY_MEDIUM;
            config->minResolutionScale = 0.6f;
            config->maxResolutionScale = 1.0f;
            config->targetFPS = 60;
            config->maxBatchSize = 128;
            config->texturePoolSize = 192;
            config->enableInstancing = true;
            break;
            
        case 3:  // Mid
            config->quality = VELOCITY_QUALITY_MEDIUM;
            config->minResolutionScale = 0.5f;
            config->maxResolutionScale = 0.85f;
            config->targetFPS = 45;
            config->maxBatchSize = 64;
            config->texturePoolSize = 128;
            config->enableInstancing = true;
            break;
            
        case 2:  // Lower mid
            config->quality = VELOCITY_QUALITY_LOW;
            config->minResolutionScale = 0.4f;
            config->maxResolutionScale = 0.7f;
            config->targetFPS = 30;
            config->maxBatchSize = 32;
            config->texturePoolSize = 64;
            config->enableInstancing = false;
            break;
            
        default:  // Low-end
            config->quality = VELOCITY_QUALITY_ULTRA_LOW;
            config->minResolutionScale = 0.3f;
            config->maxResolutionScale = 0.5f;
            config->targetFPS = 30;
            config->maxBatchSize = 16;
            config->texturePoolSize = 32;
            config->enableInstancing = false;
            break;
    }
    
    // Always enable shader caching
    config->shaderCache = VELOCITY_CACHE_DISK;
    config->enableGPUSpecificTweaks = true;
}
