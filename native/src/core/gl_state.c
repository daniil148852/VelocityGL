/**
 * GL State Tracking and Management
 */

#include "gl_wrapper.h"
#include "../utils/log.h"

#include <string.h>

// ============================================================================
// Forward Declarations
// ============================================================================

void glStateApply(const GLState* state);

// ============================================================================
// State Comparison Helpers
// ============================================================================

static inline bool floatEquals(float a, float b) {
    return (a > b ? a - b : b - a) < 0.0001f;
}

// ============================================================================
// State Stack
// ============================================================================

#define MAX_STATE_STACK 16

static GLState g_stateStack[MAX_STATE_STACK];
static int g_stateStackTop = 0;

void glStatePush(void) {
    if (!g_wrapperCtx) return;
    
    if (g_stateStackTop >= MAX_STATE_STACK) {
        velocityLogWarn("State stack overflow");
        return;
    }
    
    memcpy(&g_stateStack[g_stateStackTop], &g_wrapperCtx->state, sizeof(GLState));
    g_stateStackTop++;
}

void glStatePop(void) {
    if (!g_wrapperCtx) return;
    
    if (g_stateStackTop <= 0) {
        velocityLogWarn("State stack underflow");
        return;
    }
    
    g_stateStackTop--;
    glStateApply(&g_stateStack[g_stateStackTop]);
}

// ============================================================================
// State Application
// ============================================================================

void glStateApply(const GLState* state) {
    if (!g_wrapperCtx || !state) return;
    
    GLState* cur = &g_wrapperCtx->state;
    
    // Blend state
    if (cur->blend.enabled != state->blend.enabled) {
        if (state->blend.enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        cur->blend.enabled = state->blend.enabled;
    }
    
    if (cur->blend.srcRGB != state->blend.srcRGB ||
        cur->blend.dstRGB != state->blend.dstRGB ||
        cur->blend.srcAlpha != state->blend.srcAlpha ||
        cur->blend.dstAlpha != state->blend.dstAlpha) {
        glBlendFuncSeparate(state->blend.srcRGB, state->blend.dstRGB,
                            state->blend.srcAlpha, state->blend.dstAlpha);
        cur->blend.srcRGB = state->blend.srcRGB;
        cur->blend.dstRGB = state->blend.dstRGB;
        cur->blend.srcAlpha = state->blend.srcAlpha;
        cur->blend.dstAlpha = state->blend.dstAlpha;
    }
    
    if (cur->blend.modeRGB != state->blend.modeRGB ||
        cur->blend.modeAlpha != state->blend.modeAlpha) {
        glBlendEquationSeparate(state->blend.modeRGB, state->blend.modeAlpha);
        cur->blend.modeRGB = state->blend.modeRGB;
        cur->blend.modeAlpha = state->blend.modeAlpha;
    }
    
    // Depth state
    if (cur->depth.testEnabled != state->depth.testEnabled) {
        if (state->depth.testEnabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        cur->depth.testEnabled = state->depth.testEnabled;
    }
    
    if (cur->depth.writeEnabled != state->depth.writeEnabled) {
        glDepthMask(state->depth.writeEnabled);
        cur->depth.writeEnabled = state->depth.writeEnabled;
    }
    
    if (cur->depth.func != state->depth.func) {
        glDepthFunc(state->depth.func);
        cur->depth.func = state->depth.func;
    }
    
    // Cull state
    if (cur->rasterizer.cullFaceEnabled != state->rasterizer.cullFaceEnabled) {
        if (state->rasterizer.cullFaceEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        cur->rasterizer.cullFaceEnabled = state->rasterizer.cullFaceEnabled;
    }
    
    if (cur->rasterizer.cullMode != state->rasterizer.cullMode) {
        glCullFace(state->rasterizer.cullMode);
        cur->rasterizer.cullMode = state->rasterizer.cullMode;
    }
    
    if (cur->rasterizer.frontFace != state->rasterizer.frontFace) {
        glFrontFace(state->rasterizer.frontFace);
        cur->rasterizer.frontFace = state->rasterizer.frontFace;
    }
    
    // Scissor state
    if (cur->rasterizer.scissorEnabled != state->rasterizer.scissorEnabled) {
        if (state->rasterizer.scissorEnabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        cur->rasterizer.scissorEnabled = state->rasterizer.scissorEnabled;
    }
    
    // Viewport
    if (cur->rasterizer.viewport[0] != state->rasterizer.viewport[0] ||
        cur->rasterizer.viewport[1] != state->rasterizer.viewport[1] ||
        cur->rasterizer.viewport[2] != state->rasterizer.viewport[2] ||
        cur->rasterizer.viewport[3] != state->rasterizer.viewport[3]) {
        glViewport(state->rasterizer.viewport[0], state->rasterizer.viewport[1],
                   state->rasterizer.viewport[2], state->rasterizer.viewport[3]);
        memcpy(cur->rasterizer.viewport, state->rasterizer.viewport, sizeof(cur->rasterizer.viewport));
    }
    
    // Program
    if (cur->currentProgram != state->currentProgram) {
        glUseProgram(state->currentProgram);
        cur->currentProgram = state->currentProgram;
    }
    
    // VAO
    if (cur->vertexArray != state->vertexArray) {
        glBindVertexArray(state->vertexArray);
        cur->vertexArray = state->vertexArray;
    }
}

// ============================================================================
// State Getters
// ============================================================================

bool glStateGetBlendEnabled(void) {
    return g_wrapperCtx ? g_wrapperCtx->state.blend.enabled : false;
}

bool glStateGetDepthTestEnabled(void) {
    return g_wrapperCtx ? g_wrapperCtx->state.depth.testEnabled : false;
}

bool glStateGetDepthWriteEnabled(void) {
    return g_wrapperCtx ? g_wrapperCtx->state.depth.writeEnabled : true;
}

GLuint glStateGetCurrentProgram(void) {
    return g_wrapperCtx ? g_wrapperCtx->state.currentProgram : 0;
}

GLuint glStateGetCurrentVAO(void) {
    return g_wrapperCtx ? g_wrapperCtx->state.vertexArray : 0;
}

GLuint glStateGetBoundTexture(GLenum target, int unit) {
    if (!g_wrapperCtx || unit < 0 || unit >= MAX_TEXTURE_UNITS) return 0;
    
    switch (target) {
        case GL_TEXTURE_2D:
            return g_wrapperCtx->state.textureUnits[unit].texture2D;
        case GL_TEXTURE_3D:
            return g_wrapperCtx->state.textureUnits[unit].texture3D;
        case GL_TEXTURE_CUBE_MAP:
            return g_wrapperCtx->state.textureUnits[unit].textureCube;
        default:
            return 0;
    }
}

// ============================================================================
// State Invalidation
// ============================================================================

void glStateInvalidate(void) {
    if (!g_wrapperCtx) return;
    
    // Reset tracked state to unknown values to force re-application
    memset(&g_wrapperCtx->state, 0xFF, sizeof(GLState));
    
    velocityLogDebug("State invalidated");
}

void glStateInvalidateTextures(void) {
    if (!g_wrapperCtx) return;
    
    for (int i = 0; i < MAX_TEXTURE_UNITS; i++) {
        g_wrapperCtx->state.textureUnits[i].texture2D = 0xFFFFFFFF;
        g_wrapperCtx->state.textureUnits[i].texture3D = 0xFFFFFFFF;
        g_wrapperCtx->state.textureUnits[i].textureCube = 0xFFFFFFFF;
    }
}

void glStateInvalidateBuffers(void) {
    if (!g_wrapperCtx) return;
    
    g_wrapperCtx->state.buffers.arrayBuffer = 0xFFFFFFFF;
    g_wrapperCtx->state.buffers.elementBuffer = 0xFFFFFFFF;
    g_wrapperCtx->state.buffers.uniformBuffer = 0xFFFFFFFF;
    g_wrapperCtx->state.vertexArray = 0xFFFFFFFF;
}
