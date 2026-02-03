/**
 * Frame Pacing - Stub implementation
 */

#include "../core/gl_wrapper.h"
#include "../utils/log.h"

#include <time.h>

// ============================================================================
// Frame Timing
// ============================================================================

static uint64_t g_lastFrameTime = 0;
static float g_targetFrameTime = 16.666f; // 60 FPS

void framePacingSetTargetFPS(int fps) {
    if (fps > 0) {
        g_targetFrameTime = 1000.0f / fps;
    }
}

void framePacingBeginFrame(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_lastFrameTime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void framePacingEndFrame(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t currentTime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    float frameTimeMs = (currentTime - g_lastFrameTime) / 1000000.0f;
    
    // Simple frame pacing - could add sleep here if frame is too fast
    (void)frameTimeMs;
}
