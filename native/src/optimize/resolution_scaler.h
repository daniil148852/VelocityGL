/**
 * Resolution Scaler - Dynamic resolution scaling for consistent FPS
 */

#ifndef RESOLUTION_SCALER_H
#define RESOLUTION_SCALER_H

#include <GLES3/gl32.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define SCALER_MIN_SCALE        0.25f
#define SCALER_MAX_SCALE        2.0f
#define SCALER_DEFAULT_SCALE    1.0f
#define SCALER_HISTORY_SIZE     60      // Frames to average
#define SCALER_ADJUST_THRESHOLD 0.1f    // 10% deviation to trigger adjustment

// ============================================================================
// Types
// ============================================================================

/**
 * Upscaling algorithm
 */
typedef enum UpscaleMethod {
    UPSCALE_NEAREST,            // Fastest, pixelated
    UPSCALE_BILINEAR,           // Good balance
    UPSCALE_BICUBIC,            // Smoother
    UPSCALE_FSR,                // AMD FidelityFX (if available)
    UPSCALE_CAS                 // Contrast Adaptive Sharpening
} UpscaleMethod;

/**
 * Scaler configuration
 */
typedef struct ScalerConfig {
    bool enabled;
    float minScale;
    float maxScale;
    int targetFPS;
    float adjustSpeed;          // How fast to adjust (0.0-1.0)
    UpscaleMethod upscaleMethod;
    bool sharpening;
    float sharpenAmount;        // 0.0-1.0
} ScalerConfig;

/**
 * Resolution scaler context
 */
typedef struct ResolutionScalerContext {
    // Configuration
    ScalerConfig config;
    
    // Current state
    float currentScale;
    int nativeWidth;
    int nativeHeight;
    int renderWidth;
    int renderHeight;
    
    // Framebuffers
    GLuint renderFBO;
    GLuint renderColorTex;
    GLuint renderDepthTex;
    GLuint upscaleFBO;          // For multi-pass upscaling
    GLuint upscaleColorTex;
    
    // Shaders
    GLuint upscaleProgram;
    GLuint sharpenProgram;
    GLuint fsrProgram;
    
    // Uniforms
    GLint upscaleTexSizeLoc;
    GLint upscaleScaleLoc;
    GLint sharpenAmountLoc;
    
    // Frame time history for adaptive scaling
    float frameTimeHistory[SCALER_HISTORY_SIZE];
    int historyIndex;
    float avgFrameTime;
    
    // Statistics
    float actualFPS;
    float targetFrameTime;
    uint32_t scaleChanges;
    
    bool initialized;
} ResolutionScalerContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize resolution scaler
 */
bool resolutionScalerInit(int nativeWidth, int nativeHeight, const ScalerConfig* config);

/**
 * Shutdown resolution scaler
 */
void resolutionScalerShutdown(void);

/**
 * Resize native resolution
 */
void resolutionScalerResize(int nativeWidth, int nativeHeight);

/**
 * Set configuration
 */
void resolutionScalerSetConfig(const ScalerConfig* config);

/**
 * Get current configuration
 */
ScalerConfig resolutionScalerGetConfig(void);

/**
 * Begin frame - bind render FBO
 * Returns render dimensions
 */
void resolutionScalerBeginFrame(int* outWidth, int* outHeight);

/**
 * End frame - upscale and present
 */
void resolutionScalerEndFrame(void);

/**
 * Record frame time for adaptive scaling
 */
void resolutionScalerRecordFrameTime(float frameTimeMs);

/**
 * Force specific scale
 */
void resolutionScalerSetScale(float scale);

/**
 * Get current scale
 */
float resolutionScalerGetScale(void);

/**
 * Get render dimensions
 */
void resolutionScalerGetRenderSize(int* width, int* height);

/**
 * Get native dimensions
 */
void resolutionScalerGetNativeSize(int* width, int* height);

/**
 * Enable/disable adaptive scaling
 */
void resolutionScalerSetEnabled(bool enabled);

/**
 * Check if enabled
 */
bool resolutionScalerIsEnabled(void);

// ============================================================================
// Upscaling Methods
// ============================================================================

/**
 * Set upscaling method
 */
void resolutionScalerSetUpscaleMethod(UpscaleMethod method);

/**
 * Get upscaling method
 */
UpscaleMethod resolutionScalerGetUpscaleMethod(void);

/**
 * Set sharpening
 */
void resolutionScalerSetSharpening(bool enabled, float amount);

// ============================================================================
// Statistics
// ============================================================================

/**
 * Get actual FPS
 */
float resolutionScalerGetActualFPS(void);

/**
 * Get number of scale changes
 */
uint32_t resolutionScalerGetScaleChanges(void);

#ifdef __cplusplus
}
#endif

#endif // RESOLUTION_SCALER_H
