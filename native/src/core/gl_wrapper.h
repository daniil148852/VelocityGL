/**
 * GL Wrapper - Internal header
 * Core OpenGL -> OpenGL ES translation layer
 */

#ifndef GL_WRAPPER_H
#define GL_WRAPPER_H

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdbool.h>
#include <stdint.h>

#include "velocity_gl.h"

// ============================================================================
// Internal Macros
// ============================================================================

#define GL_STACK_SIZE 32
#define MAX_TEXTURE_UNITS 32
#define MAX_VERTEX_ATTRIBS 16
#define MAX_UNIFORM_BUFFERS 16
#define MAX_SHADER_STORAGE_BUFFERS 8

// Error checking macros
#ifdef VELOCITY_DEBUG
    #define GL_CHECK_ERROR() glWrapperCheckError(__FILE__, __LINE__)
    #define VELOCITY_ASSERT(cond, msg) \
        if (!(cond)) { velocityLogError("Assert failed: %s at %s:%d", msg, __FILE__, __LINE__); }
#else
    #define GL_CHECK_ERROR()
    #define VELOCITY_ASSERT(cond, msg)
#endif

// ============================================================================
// OpenGL State Tracking
// ============================================================================

/**
 * Tracked blend state
 */
typedef struct GLBlendState {
    bool enabled;
    GLenum srcRGB;
    GLenum dstRGB;
    GLenum srcAlpha;
    GLenum dstAlpha;
    GLenum modeRGB;
    GLenum modeAlpha;
    float color[4];
} GLBlendState;

/**
 * Tracked depth state
 */
typedef struct GLDepthState {
    bool testEnabled;
    bool writeEnabled;
    GLenum func;
    float rangeNear;
    float rangeFar;
    double clearValue;
} GLDepthState;

/**
 * Tracked stencil state
 */
typedef struct GLStencilState {
    bool enabled;
    GLenum func;
    GLint ref;
    GLuint mask;
    GLuint writeMask;
    GLenum sfail;
    GLenum dpfail;
    GLenum dppass;
} GLStencilState;

/**
 * Tracked rasterizer state
 */
typedef struct GLRasterizerState {
    bool cullFaceEnabled;
    GLenum cullMode;
    GLenum frontFace;
    GLenum polygonMode;  // Emulated on ES
    float lineWidth;
    float pointSize;
    bool scissorEnabled;
    GLint scissor[4];
    GLint viewport[4];
    bool depthClampEnabled;
    bool rasterizerDiscardEnabled;
} GLRasterizerState;

/**
 * Tracked texture unit state
 */
typedef struct GLTextureUnitState {
    GLuint texture2D;
    GLuint texture3D;
    GLuint textureCube;
    GLuint texture2DArray;
    GLuint sampler;
} GLTextureUnitState;

/**
 * Tracked buffer bindings
 */
typedef struct GLBufferBindings {
    GLuint arrayBuffer;
    GLuint elementBuffer;
    GLuint uniformBuffer;
    GLuint shaderStorageBuffer;
    GLuint copyReadBuffer;
    GLuint copyWriteBuffer;
    GLuint pixelPackBuffer;
    GLuint pixelUnpackBuffer;
    GLuint transformFeedbackBuffer;
    GLuint dispatchIndirectBuffer;
    GLuint drawIndirectBuffer;
} GLBufferBindings;

/**
 * Tracked framebuffer state
 */
typedef struct GLFramebufferState {
    GLuint drawFramebuffer;
    GLuint readFramebuffer;
    GLuint renderbuffer;
    GLenum drawBuffers[8];
    int numDrawBuffers;
} GLFramebufferState;

/**
 * Matrix stack (for legacy GL compatibility)
 */
typedef struct GLMatrixStack {
    float stack[GL_STACK_SIZE][16];
    int top;
} GLMatrixStack;

/**
 * Complete tracked GL state
 */
typedef struct GLState {
    // Blend
    GLBlendState blend;
    
    // Depth
    GLDepthState depth;
    
    // Stencil
    GLStencilState stencilFront;
    GLStencilState stencilBack;
    
    // Rasterizer
    GLRasterizerState rasterizer;
    
    // Textures
    GLint activeTextureUnit;
    GLTextureUnitState textureUnits[MAX_TEXTURE_UNITS];
    
    // Buffers
    GLBufferBindings buffers;
    GLuint vertexArray;
    
    // Framebuffers
    GLFramebufferState framebuffer;
    
    // Program
    GLuint currentProgram;
    
    // Legacy matrix stacks (for old GL)
    GLMatrixStack modelViewStack;
    GLMatrixStack projectionStack;
    GLMatrixStack textureStack;
    GLenum matrixMode;
    
    // Clear values
    float clearColor[4];
    float clearDepth;
    int clearStencil;
    
    // Misc
    bool multisampleEnabled;
    bool srgbEnabled;
    GLuint packAlignment;
    GLuint unpackAlignment;
    
} GLState;

// ============================================================================
// Global State
// ============================================================================

/**
 * Main wrapper context
 */
typedef struct GLWrapperContext {
    // State tracking
    GLState state;
    GLState savedState;  // For push/pop
    
    // Configuration
    VelocityConfig config;
    VelocityGPUCaps gpuCaps;
    
    // Statistics
    VelocityStats stats;
    
    // EGL handles
    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLContext eglContext;
    EGLConfig eglConfig;
    
    // Native window
    void* nativeWindow;
    int windowWidth;
    int windowHeight;
    
    // Thread safety
    volatile bool initialized;
    volatile bool contextCurrent;
    
} GLWrapperContext;

// Global wrapper context
extern GLWrapperContext* g_wrapperCtx;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * Initialize the wrapper
 */
bool glWrapperInit(const VelocityConfig* config);

/**
 * Shutdown wrapper
 */
void glWrapperShutdown(void);

/**
 * Create EGL context
 */
bool glWrapperCreateContext(void* nativeWindow, EGLDisplay display);

/**
 * Destroy EGL context
 */
void glWrapperDestroyContext(void);

/**
 * Make context current
 */
bool glWrapperMakeCurrent(void);

/**
 * Swap buffers
 */
void glWrapperSwapBuffers(void);

/**
 * Reset state to defaults
 */
void glWrapperResetState(void);

/**
 * Check for GL errors
 */
void glWrapperCheckError(const char* file, int line);

/**
 * Get GL function pointer
 */
void* glWrapperGetProcAddress(const char* name);

// ============================================================================
// State Management
// ============================================================================

/**
 * Save current state
 */
void glWrapperPushState(void);

/**
 * Restore saved state
 */
void glWrapperPopState(void);

/**
 * Apply state delta (only changed state)
 */
void glWrapperApplyStateDelta(const GLState* newState);

// ============================================================================
// Statistics
// ============================================================================

/**
 * Record a draw call
 */
void glWrapperRecordDrawCall(int vertices, int instances);

/**
 * Begin frame timing
 */
void glWrapperBeginFrame(void);

/**
 * End frame timing
 */
void glWrapperEndFrame(void);

#endif // GL_WRAPPER_H
