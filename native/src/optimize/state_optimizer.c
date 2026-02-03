/**
 * State Optimizer - Reduces redundant state changes
 */

#include "../core/gl_wrapper.h"
#include "../utils/log.h"

// ============================================================================
// State Change Counting
// ============================================================================

static uint32_t g_stateChanges = 0;
static uint32_t g_stateChangesAvoided = 0;

void stateOptimizerReset(void) {
    g_stateChanges = 0;
    g_stateChangesAvoided = 0;
}

void stateOptimizerGetStats(uint32_t* changes, uint32_t* avoided) {
    if (changes) *changes = g_stateChanges;
    if (avoided) *avoided = g_stateChangesAvoided;
}

// ============================================================================
// Optimized State Setting
// ============================================================================

bool stateOptimizerSetBlend(bool enable) {
    if (!g_wrapperCtx) return false;
    
    if (g_wrapperCtx->state.blend.enabled == enable) {
        g_stateChangesAvoided++;
        return false;
    }
    
    g_stateChanges++;
    return true;
}

bool stateOptimizerSetDepthTest(bool enable) {
    if (!g_wrapperCtx) return false;
    
    if (g_wrapperCtx->state.depth.testEnabled == enable) {
        g_stateChangesAvoided++;
        return false;
    }
    
    g_stateChanges++;
    return true;
}

bool stateOptimizerSetProgram(GLuint program) {
    if (!g_wrapperCtx) return false;
    
    if (g_wrapperCtx->state.currentProgram == program) {
        g_stateChangesAvoided++;
        return false;
    }
    
    g_stateChanges++;
    return true;
}
