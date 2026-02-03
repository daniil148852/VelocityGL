/**
 * GL Context Management
 */

#include "gl_wrapper.h"
#include "../utils/log.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string.h>

// ============================================================================
// EGL Extension Function Pointers
// ============================================================================

static PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR = NULL;
static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = NULL;
static PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR = NULL;

// ============================================================================
// Context Configuration
// ============================================================================

static const EGLint DEFAULT_CONTEXT_ATTRIBS_ES32[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 2,
    EGL_NONE
};

static const EGLint DEFAULT_CONTEXT_ATTRIBS_ES31[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 1,
    EGL_NONE
};

static const EGLint DEFAULT_CONTEXT_ATTRIBS_ES30[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 0,
    EGL_NONE
};

static const EGLint DEFAULT_CONFIG_ATTRIBS[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_NONE
};

// ============================================================================
// Extension Loading
// ============================================================================

void glContextLoadExtensions(void) {
    eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    
    velocityLogDebug("EGL extensions loaded");
}

// ============================================================================
// Config Selection
// ============================================================================

EGLConfig glContextChooseConfig(EGLDisplay display, const EGLint* attribs) {
    EGLConfig config;
    EGLint numConfigs;
    
    const EGLint* configAttribs = attribs ? attribs : DEFAULT_CONFIG_ATTRIBS;
    
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        velocityLogError("eglChooseConfig failed");
        return NULL;
    }
    
    return config;
}

// ============================================================================
// Context Creation
// ============================================================================

EGLContext glContextCreate(EGLDisplay display, EGLConfig config, EGLContext shareContext) {
    EGLContext context;
    
    // Try ES 3.2 first
    context = eglCreateContext(display, config, shareContext, DEFAULT_CONTEXT_ATTRIBS_ES32);
    if (context != EGL_NO_CONTEXT) {
        velocityLogInfo("Created OpenGL ES 3.2 context");
        return context;
    }
    
    // Fall back to ES 3.1
    context = eglCreateContext(display, config, shareContext, DEFAULT_CONTEXT_ATTRIBS_ES31);
    if (context != EGL_NO_CONTEXT) {
        velocityLogInfo("Created OpenGL ES 3.1 context");
        return context;
    }
    
    // Fall back to ES 3.0
    context = eglCreateContext(display, config, shareContext, DEFAULT_CONTEXT_ATTRIBS_ES30);
    if (context != EGL_NO_CONTEXT) {
        velocityLogInfo("Created OpenGL ES 3.0 context");
        return context;
    }
    
    velocityLogError("Failed to create any OpenGL ES 3.x context");
    return EGL_NO_CONTEXT;
}

// ============================================================================
// Surface Creation
// ============================================================================

EGLSurface glContextCreateSurface(EGLDisplay display, EGLConfig config, void* nativeWindow) {
    EGLSurface surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)nativeWindow, NULL);
    
    if (surface == EGL_NO_SURFACE) {
        EGLint error = eglGetError();
        velocityLogError("eglCreateWindowSurface failed: 0x%x", error);
        return EGL_NO_SURFACE;
    }
    
    return surface;
}

// ============================================================================
// Sync Objects
// ============================================================================

void* glContextCreateSync(EGLDisplay display) {
    if (eglCreateSyncKHR) {
        return eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, NULL);
    }
    return NULL;
}

void glContextDestroySync(EGLDisplay display, void* sync) {
    if (eglDestroySyncKHR && sync) {
        eglDestroySyncKHR(display, (EGLSyncKHR)sync);
    }
}

bool glContextWaitSync(EGLDisplay display, void* sync, uint64_t timeout) {
    if (eglClientWaitSyncKHR && sync) {
        EGLint result = eglClientWaitSyncKHR(display, (EGLSyncKHR)sync, 
                                              EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, timeout);
        return result == EGL_CONDITION_SATISFIED_KHR;
    }
    return true;
}

// ============================================================================
// Swap Interval
// ============================================================================

void glContextSetSwapInterval(EGLDisplay display, int interval) {
    eglSwapInterval(display, interval);
}

// ============================================================================
// Query Functions
// ============================================================================

void glContextGetSurfaceSize(EGLDisplay display, EGLSurface surface, int* width, int* height) {
    if (width) {
        eglQuerySurface(display, surface, EGL_WIDTH, width);
    }
    if (height) {
        eglQuerySurface(display, surface, EGL_HEIGHT, height);
    }
}

const char* glContextGetEGLVersion(EGLDisplay display) {
    return eglQueryString(display, EGL_VERSION);
}

const char* glContextGetEGLVendor(EGLDisplay display) {
    return eglQueryString(display, EGL_VENDOR);
}

const char* glContextGetEGLExtensions(EGLDisplay display) {
    return eglQueryString(display, EGL_EXTENSIONS);
}
