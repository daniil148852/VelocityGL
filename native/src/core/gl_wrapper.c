/**
 * GL Wrapper - Core implementation
 */

#include "gl_wrapper.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../shader/shader_cache.h"
#include "../gpu/gpu_detect.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// ============================================================================
// Global State
// ============================================================================

GLWrapperContext* g_wrapperCtx = NULL;
static pthread_mutex_t g_initMutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// Default State Values
// ============================================================================

static const GLState DEFAULT_STATE = {
    // Blend
    .blend = {
        .enabled = false,
        .srcRGB = GL_ONE,
        .dstRGB = GL_ZERO,
        .srcAlpha = GL_ONE,
        .dstAlpha = GL_ZERO,
        .modeRGB = GL_FUNC_ADD,
        .modeAlpha = GL_FUNC_ADD,
        .color = {0.0f, 0.0f, 0.0f, 0.0f}
    },
    
    // Depth
    .depth = {
        .testEnabled = false,
        .writeEnabled = true,
        .func = GL_LESS,
        .rangeNear = 0.0f,
        .rangeFar = 1.0f,
        .clearValue = 1.0
    },
    
    // Stencil front
    .stencilFront = {
        .enabled = false,
        .func = GL_ALWAYS,
        .ref = 0,
        .mask = 0xFFFFFFFF,
        .writeMask = 0xFFFFFFFF,
        .sfail = GL_KEEP,
        .dpfail = GL_KEEP,
        .dppass = GL_KEEP
    },
    
    // Stencil back
    .stencilBack = {
        .enabled = false,
        .func = GL_ALWAYS,
        .ref = 0,
        .mask = 0xFFFFFFFF,
        .writeMask = 0xFFFFFFFF,
        .sfail = GL_KEEP,
        .dpfail = GL_KEEP,
        .dppass = GL_KEEP
    },
    
    // Rasterizer
    .rasterizer = {
        .cullFaceEnabled = false,
        .cullMode = GL_BACK,
        .frontFace = GL_CCW,
        .polygonMode = GL_FILL,
        .lineWidth = 1.0f,
        .pointSize = 1.0f,
        .scissorEnabled = false,
        .scissor = {0, 0, 0, 0},
        .viewport = {0, 0, 0, 0},
        .depthClampEnabled = false,
        .rasterizerDiscardEnabled = false
    },
    
    // Textures
    .activeTextureUnit = 0,
    
    // Framebuffers
    .framebuffer = {
        .drawFramebuffer = 0,
        .readFramebuffer = 0,
        .renderbuffer = 0,
        .numDrawBuffers = 1
    },
    
    // Program
    .currentProgram = 0,
    
    // Matrix mode (legacy)
    .matrixMode = GL_MODELVIEW,
    
    // Clear values
    .clearColor = {0.0f, 0.0f, 0.0f, 0.0f},
    .clearDepth = 1.0f,
    .clearStencil = 0,
    
    // Misc
    .multisampleEnabled = false,
    .srgbEnabled = false,
    .packAlignment = 4,
    .unpackAlignment = 4
};

// ============================================================================
// Identity Matrix
// ============================================================================

static const float IDENTITY_MATRIX[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

// ============================================================================
// Initialization
// ============================================================================

bool glWrapperInit(const VelocityConfig* config) {
    pthread_mutex_lock(&g_initMutex);
    
    if (g_wrapperCtx != NULL) {
        velocityLogWarn("GL Wrapper already initialized");
        pthread_mutex_unlock(&g_initMutex);
        return true;
    }
    
    velocityLogInfo("Initializing VelocityGL v%s", VELOCITY_VERSION_STRING);
    
    // Allocate context
    g_wrapperCtx = (GLWrapperContext*)velocityMalloc(sizeof(GLWrapperContext));
    if (!g_wrapperCtx) {
        velocityLogError("Failed to allocate wrapper context");
        pthread_mutex_unlock(&g_initMutex);
        return false;
    }
    
    memset(g_wrapperCtx, 0, sizeof(GLWrapperContext));
    
    // Copy configuration
    if (config) {
        memcpy(&g_wrapperCtx->config, config, sizeof(VelocityConfig));
    } else {
        g_wrapperCtx->config = velocityGetDefaultConfig();
    }
    
    // Initialize state to defaults
    glWrapperResetState();
    
    // Initialize matrix stacks
    for (int i = 0; i < GL_STACK_SIZE; i++) {
        memcpy(g_wrapperCtx->state.modelViewStack.stack[i], IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
        memcpy(g_wrapperCtx->state.projectionStack.stack[i], IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
        memcpy(g_wrapperCtx->state.textureStack.stack[i], IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
    }
    
    // Initialize subsystems
    if (g_wrapperCtx->config.shaderCache != VELOCITY_CACHE_DISABLED) {
        shaderCacheInit(g_wrapperCtx->config.shaderCachePath, 
                        g_wrapperCtx->config.shaderCacheMaxSize);
    }
    
    g_wrapperCtx->initialized = true;
    
    velocityLogInfo("VelocityGL initialized successfully");
    pthread_mutex_unlock(&g_initMutex);
    
    return true;
}

void glWrapperShutdown(void) {
    pthread_mutex_lock(&g_initMutex);
    
    if (!g_wrapperCtx) {
        pthread_mutex_unlock(&g_initMutex);
        return;
    }
    
    velocityLogInfo("Shutting down VelocityGL");
    
    // Destroy context if exists
    if (g_wrapperCtx->contextCurrent) {
        glWrapperDestroyContext();
    }
    
    // Shutdown subsystems
    shaderCacheShutdown();
    
    // Free context
    velocityFree(g_wrapperCtx);
    g_wrapperCtx = NULL;
    
    velocityLogInfo("VelocityGL shutdown complete");
    pthread_mutex_unlock(&g_initMutex);
}

// ============================================================================
// Context Management
// ============================================================================

bool glWrapperCreateContext(void* nativeWindow, EGLDisplay display) {
    if (!g_wrapperCtx || !g_wrapperCtx->initialized) {
        velocityLogError("Wrapper not initialized");
        return false;
    }
    
    if (g_wrapperCtx->contextCurrent) {
        velocityLogWarn("Context already created");
        return true;
    }
    
    g_wrapperCtx->nativeWindow = nativeWindow;
    g_wrapperCtx->eglDisplay = display;
    
    // Choose EGL config
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };
    
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &g_wrapperCtx->eglConfig, 1, &numConfigs) || numConfigs == 0) {
        velocityLogError("Failed to choose EGL config");
        return false;
    }
    
    // Create window surface
    g_wrapperCtx->eglSurface = eglCreateWindowSurface(display, g_wrapperCtx->eglConfig, 
                                                       (EGLNativeWindowType)nativeWindow, NULL);
    if (g_wrapperCtx->eglSurface == EGL_NO_SURFACE) {
        velocityLogError("Failed to create EGL surface");
        return false;
    }
    
    // Create context with ES 3.2
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    
    g_wrapperCtx->eglContext = eglCreateContext(display, g_wrapperCtx->eglConfig, 
                                                 EGL_NO_CONTEXT, contextAttribs);
    
    // Fallback to ES 3.1 if 3.2 not available
    if (g_wrapperCtx->eglContext == EGL_NO_CONTEXT) {
        contextAttribs[3] = 1;
        g_wrapperCtx->eglContext = eglCreateContext(display, g_wrapperCtx->eglConfig, 
                                                     EGL_NO_CONTEXT, contextAttribs);
    }
    
    // Fallback to ES 3.0
    if (g_wrapperCtx->eglContext == EGL_NO_CONTEXT) {
        contextAttribs[3] = 0;
        g_wrapperCtx->eglContext = eglCreateContext(display, g_wrapperCtx->eglConfig, 
                                                     EGL_NO_CONTEXT, contextAttribs);
    }
    
    if (g_wrapperCtx->eglContext == EGL_NO_CONTEXT) {
        velocityLogError("Failed to create EGL context");
        eglDestroySurface(display, g_wrapperCtx->eglSurface);
        return false;
    }
    
    // Make current
    if (!glWrapperMakeCurrent()) {
        velocityLogError("Failed to make context current");
        eglDestroyContext(display, g_wrapperCtx->eglContext);
        eglDestroySurface(display, g_wrapperCtx->eglSurface);
        return false;
    }
    
    // Detect GPU and capabilities
    gpuDetect(&g_wrapperCtx->gpuCaps);
    
    velocityLogInfo("Created OpenGL ES context:");
    velocityLogInfo("  Vendor: %s", g_wrapperCtx->gpuCaps.vendorString);
    velocityLogInfo("  Renderer: %s", g_wrapperCtx->gpuCaps.rendererString);
    velocityLogInfo("  Version: %s", g_wrapperCtx->gpuCaps.versionString);
    velocityLogInfo("  Emulating: OpenGL %d.%d", 
                    g_wrapperCtx->gpuCaps.glVersionMajor,
                    g_wrapperCtx->gpuCaps.glVersionMinor);
    
    // Get window size
    eglQuerySurface(display, g_wrapperCtx->eglSurface, EGL_WIDTH, &g_wrapperCtx->windowWidth);
    eglQuerySurface(display, g_wrapperCtx->eglSurface, EGL_HEIGHT, &g_wrapperCtx->windowHeight);
    
    // Apply GPU-specific tweaks
    if (g_wrapperCtx->config.enableGPUSpecificTweaks) {
        gpuApplyTweaks(g_wrapperCtx->gpuCaps.vendor);
    }
    
    g_wrapperCtx->contextCurrent = true;
    
    return true;
}

void glWrapperDestroyContext(void) {
    if (!g_wrapperCtx) return;
    
    if (g_wrapperCtx->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_wrapperCtx->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        if (g_wrapperCtx->eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(g_wrapperCtx->eglDisplay, g_wrapperCtx->eglContext);
            g_wrapperCtx->eglContext = EGL_NO_CONTEXT;
        }
        
        if (g_wrapperCtx->eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(g_wrapperCtx->eglDisplay, g_wrapperCtx->eglSurface);
            g_wrapperCtx->eglSurface = EGL_NO_SURFACE;
        }
    }
    
    g_wrapperCtx->contextCurrent = false;
}

bool glWrapperMakeCurrent(void) {
    if (!g_wrapperCtx) return false;
    
    return eglMakeCurrent(g_wrapperCtx->eglDisplay, 
                          g_wrapperCtx->eglSurface, 
                          g_wrapperCtx->eglSurface, 
                          g_wrapperCtx->eglContext) == EGL_TRUE;
}

void glWrapperSwapBuffers(void) {
    if (!g_wrapperCtx || !g_wrapperCtx->contextCurrent) return;
    
    eglSwapBuffers(g_wrapperCtx->eglDisplay, g_wrapperCtx->eglSurface);
}

// ============================================================================
// State Management
// ============================================================================

void glWrapperResetState(void) {
    if (!g_wrapperCtx) return;
    
    memcpy(&g_wrapperCtx->state, &DEFAULT_STATE, sizeof(GLState));
}

void glWrapperPushState(void) {
    if (!g_wrapperCtx) return;
    
    memcpy(&g_wrapperCtx->savedState, &g_wrapperCtx->state, sizeof(GLState));
}

void glWrapperPopState(void) {
    if (!g_wrapperCtx) return;
    
    glWrapperApplyStateDelta(&g_wrapperCtx->savedState);
    memcpy(&g_wrapperCtx->state, &g_wrapperCtx->savedState, sizeof(GLState));
}

void glWrapperApplyStateDelta(const GLState* newState) {
    GLState* cur = &g_wrapperCtx->state;
    
    // Apply only changed blend state
    if (cur->blend.enabled != newState->blend.enabled) {
        if (newState->blend.enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
    }
    
    if (cur->blend.srcRGB != newState->blend.srcRGB ||
        cur->blend.dstRGB != newState->blend.dstRGB ||
        cur->blend.srcAlpha != newState->blend.srcAlpha ||
        cur->blend.dstAlpha != newState->blend.dstAlpha) {
        glBlendFuncSeparate(newState->blend.srcRGB, newState->blend.dstRGB,
                            newState->blend.srcAlpha, newState->blend.dstAlpha);
    }
    
    // Apply depth state
    if (cur->depth.testEnabled != newState->depth.testEnabled) {
        if (newState->depth.testEnabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }
    
    if (cur->depth.writeEnabled != newState->depth.writeEnabled) {
        glDepthMask(newState->depth.writeEnabled);
    }
    
    if (cur->depth.func != newState->depth.func) {
        glDepthFunc(newState->depth.func);
    }
    
    // Apply cull state
    if (cur->rasterizer.cullFaceEnabled != newState->rasterizer.cullFaceEnabled) {
        if (newState->rasterizer.cullFaceEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    
    if (cur->rasterizer.cullMode != newState->rasterizer.cullMode) {
        glCullFace(newState->rasterizer.cullMode);
    }
    
    // Continue for other states...
}

// ============================================================================
// Error Checking
// ============================================================================

void glWrapperCheckError(const char* file, int line) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        const char* errStr;
        switch (err) {
            case GL_INVALID_ENUM:      errStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     errStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errStr = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     errStr = "OUT_OF_MEMORY"; break;
            default:                   errStr = "UNKNOWN"; break;
        }
        velocityLogError("GL Error %s (0x%x) at %s:%d", errStr, err, file, line);
    }
}

// ============================================================================
// Statistics
// ============================================================================

void glWrapperRecordDrawCall(int vertices, int instances) {
    if (!g_wrapperCtx) return;
    
    g_wrapperCtx->stats.drawCalls++;
    g_wrapperCtx->stats.triangles += vertices / 3 * instances;
}

static uint64_t frameStartTime = 0;

void glWrapperBeginFrame(void) {
    if (!g_wrapperCtx) return;
    
    // Reset per-frame stats
    g_wrapperCtx->stats.drawCalls = 0;
    g_wrapperCtx->stats.triangles = 0;
    g_wrapperCtx->stats.drawCallsSaved = 0;
    
    // Start timing
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    frameStartTime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void glWrapperEndFrame(void) {
    if (!g_wrapperCtx) return;
    
    // Calculate frame time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t frameEndTime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    float frameTimeMs = (frameEndTime - frameStartTime) / 1000000.0f;
    g_wrapperCtx->stats.frameTimeMs = frameTimeMs;
    g_wrapperCtx->stats.currentFPS = 1000.0f / frameTimeMs;
    
    // Rolling average FPS
    static float fpsHistory[60] = {0};
    static int fpsIndex = 0;
    fpsHistory[fpsIndex] = g_wrapperCtx->stats.currentFPS;
    fpsIndex = (fpsIndex + 1) % 60;
    
    float sum = 0;
    for (int i = 0; i < 60; i++) {
        sum += fpsHistory[i];
    }
    g_wrapperCtx->stats.avgFPS = sum / 60.0f;
}
