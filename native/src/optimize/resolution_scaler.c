/**
 * Resolution Scaler - Implementation
 */

#include "resolution_scaler.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../core/gl_wrapper.h"

#include <string.h>
#include <math.h>

// ============================================================================
// Shader Sources
// ============================================================================

static const char* UPSCALE_VERTEX_SHADER = 
    "#version 300 es\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

static const char* UPSCALE_BILINEAR_FRAGMENT_SHADER = 
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 vTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTexture;\n"
    "void main() {\n"
    "    fragColor = texture(uTexture, vTexCoord);\n"
    "}\n";

static const char* SHARPEN_FRAGMENT_SHADER = 
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 vTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uTexelSize;\n"
    "uniform float uAmount;\n"
    "void main() {\n"
    "    vec4 color = texture(uTexture, vTexCoord);\n"
    "    vec4 blur = texture(uTexture, vTexCoord + vec2(-uTexelSize.x, 0.0)) +\n"
    "                texture(uTexture, vTexCoord + vec2(uTexelSize.x, 0.0)) +\n"
    "                texture(uTexture, vTexCoord + vec2(0.0, -uTexelSize.y)) +\n"
    "                texture(uTexture, vTexCoord + vec2(0.0, uTexelSize.y));\n"
    "    blur *= 0.25;\n"
    "    fragColor = color + (color - blur) * uAmount;\n"
    "}\n";

// CAS (Contrast Adaptive Sharpening) - simplified version
static const char* CAS_FRAGMENT_SHADER = 
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 vTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec2 uTexelSize;\n"
    "uniform float uSharpness;\n"
    "\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "\n"
    "void main() {\n"
    "    vec3 a = texture(uTexture, vTexCoord + vec2(-uTexelSize.x, -uTexelSize.y)).rgb;\n"
    "    vec3 b = texture(uTexture, vTexCoord + vec2(0.0, -uTexelSize.y)).rgb;\n"
    "    vec3 c = texture(uTexture, vTexCoord + vec2(uTexelSize.x, -uTexelSize.y)).rgb;\n"
    "    vec3 d = texture(uTexture, vTexCoord + vec2(-uTexelSize.x, 0.0)).rgb;\n"
    "    vec3 e = texture(uTexture, vTexCoord).rgb;\n"
    "    vec3 f = texture(uTexture, vTexCoord + vec2(uTexelSize.x, 0.0)).rgb;\n"
    "    vec3 g = texture(uTexture, vTexCoord + vec2(-uTexelSize.x, uTexelSize.y)).rgb;\n"
    "    vec3 h = texture(uTexture, vTexCoord + vec2(0.0, uTexelSize.y)).rgb;\n"
    "    vec3 i = texture(uTexture, vTexCoord + vec2(uTexelSize.x, uTexelSize.y)).rgb;\n"
    "\n"
    "    float mnL = min(min(min(luma(d), luma(e)), min(luma(f), luma(b))), luma(h));\n"
    "    float mxL = max(max(max(luma(d), luma(e)), max(luma(f), luma(b))), luma(h));\n"
    "    float ampL = clamp(min(mnL, 1.0 - mxL) / mxL, 0.0, 1.0);\n"
    "    ampL = sqrt(ampL) * uSharpness;\n"
    "\n"
    "    vec3 wL = vec3(-ampL * 0.25);\n"
    "    vec3 peak = vec3(1.0 + ampL * 4.0);\n"
    "\n"
    "    vec3 result = (b * wL + d * wL + f * wL + h * wL + e * peak);\n"
    "    result /= (4.0 * wL + peak);\n"
    "\n"
    "    fragColor = vec4(result, 1.0);\n"
    "}\n";

// ============================================================================
// Global State
// ============================================================================

static ResolutionScalerContext* g_scaler = NULL;

// Fullscreen quad
static GLuint g_quadVAO = 0;
static GLuint g_quadVBO = 0;

static const float QUAD_VERTICES[] = {
    // Position     // TexCoord
    -1.0f,  1.0f,   0.0f, 1.0f,
    -1.0f, -1.0f,   0.0f, 0.0f,
     1.0f, -1.0f,   1.0f, 0.0f,
    
    -1.0f,  1.0f,   0.0f, 1.0f,
     1.0f, -1.0f,   1.0f, 0.0f,
     1.0f,  1.0f,   1.0f, 1.0f
};

// ============================================================================
// Helper Functions
// ============================================================================

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        velocityLogError("Shader compilation failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

static GLuint createProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    
    if (!vert || !frag) {
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        velocityLogError("Program linking failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

static void createFramebuffers(void) {
    if (!g_scaler) return;
    
    // Delete existing
    if (g_scaler->renderFBO) {
        glDeleteFramebuffers(1, &g_scaler->renderFBO);
        glDeleteTextures(1, &g_scaler->renderColorTex);
        glDeleteTextures(1, &g_scaler->renderDepthTex);
    }
    
    // Create render FBO
    glGenFramebuffers(1, &g_scaler->renderFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_scaler->renderFBO);
    
    // Color texture
    glGenTextures(1, &g_scaler->renderColorTex);
    glBindTexture(GL_TEXTURE_2D, g_scaler->renderColorTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, g_scaler->renderWidth, g_scaler->renderHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
                           g_scaler->renderColorTex, 0);
    
    // Depth texture
    glGenTextures(1, &g_scaler->renderDepthTex);
    glBindTexture(GL_TEXTURE_2D, g_scaler->renderDepthTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 
                   g_scaler->renderWidth, g_scaler->renderHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           g_scaler->renderDepthTex, 0);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        velocityLogError("Render framebuffer incomplete: 0x%x", status);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    velocityLogInfo("Created render FBO: %dx%d (scale: %.2f)", 
                    g_scaler->renderWidth, g_scaler->renderHeight, g_scaler->currentScale);
}

static void updateRenderSize(void) {
    if (!g_scaler) return;
    
    int newWidth = (int)(g_scaler->nativeWidth * g_scaler->currentScale);
    int newHeight = (int)(g_scaler->nativeHeight * g_scaler->currentScale);
    
    // Ensure even dimensions
    newWidth = (newWidth + 1) & ~1;
    newHeight = (newHeight + 1) & ~1;
    
    // Clamp to reasonable sizes
    if (newWidth < 64) newWidth = 64;
    if (newHeight < 64) newHeight = 64;
    if (newWidth > g_scaler->nativeWidth * 2) newWidth = g_scaler->nativeWidth * 2;
    if (newHeight > g_scaler->nativeHeight * 2) newHeight = g_scaler->nativeHeight * 2;
    
    if (newWidth != g_scaler->renderWidth || newHeight != g_scaler->renderHeight) {
        g_scaler->renderWidth = newWidth;
        g_scaler->renderHeight = newHeight;
        createFramebuffers();
        g_scaler->scaleChanges++;
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool resolutionScalerInit(int nativeWidth, int nativeHeight, const ScalerConfig* config) {
    if (g_scaler) {
        velocityLogWarn("Resolution scaler already initialized");
        return true;
    }
    
    velocityLogInfo("Initializing resolution scaler (%dx%d)", nativeWidth, nativeHeight);
    
    g_scaler = (ResolutionScalerContext*)velocityCalloc(1, sizeof(ResolutionScalerContext));
    if (!g_scaler) {
        velocityLogError("Failed to allocate resolution scaler");
        return false;
    }
    
    // Set configuration
    if (config) {
        memcpy(&g_scaler->config, config, sizeof(ScalerConfig));
    } else {
        g_scaler->config.enabled = true;
        g_scaler->config.minScale = 0.5f;
        g_scaler->config.maxScale = 1.0f;
        g_scaler->config.targetFPS = 60;
        g_scaler->config.adjustSpeed = 0.1f;
        g_scaler->config.upscaleMethod = UPSCALE_BILINEAR;
        g_scaler->config.sharpening = true;
        g_scaler->config.sharpenAmount = 0.3f;
    }
    
    g_scaler->nativeWidth = nativeWidth;
    g_scaler->nativeHeight = nativeHeight;
    g_scaler->currentScale = g_scaler->config.maxScale;
    g_scaler->renderWidth = (int)(nativeWidth * g_scaler->currentScale);
    g_scaler->renderHeight = (int)(nativeHeight * g_scaler->currentScale);
    g_scaler->targetFrameTime = 1000.0f / g_scaler->config.targetFPS;
    
    // Create fullscreen quad
    glGenVertexArrays(1, &g_quadVAO);
    glGenBuffers(1, &g_quadVBO);
    
    glBindVertexArray(g_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
    
    // Create shaders
    g_scaler->upscaleProgram = createProgram(UPSCALE_VERTEX_SHADER, UPSCALE_BILINEAR_FRAGMENT_SHADER);
    g_scaler->sharpenProgram = createProgram(UPSCALE_VERTEX_SHADER, CAS_FRAGMENT_SHADER);
    
    if (!g_scaler->upscaleProgram) {
        velocityLogError("Failed to create upscale program");
        velocityFree(g_scaler);
        g_scaler = NULL;
        return false;
    }
    
    // Create framebuffers
    createFramebuffers();
    
    g_scaler->initialized = true;
    
    velocityLogInfo("Resolution scaler initialized (target: %d FPS)", g_scaler->config.targetFPS);
    
    return true;
}

void resolutionScalerShutdown(void) {
    if (!g_scaler) return;
    
    velocityLogInfo("Shutting down resolution scaler");
    
    glDeleteFramebuffers(1, &g_scaler->renderFBO);
    glDeleteTextures(1, &g_scaler->renderColorTex);
    glDeleteTextures(1, &g_scaler->renderDepthTex);
    glDeleteProgram(g_scaler->upscaleProgram);
    glDeleteProgram(g_scaler->sharpenProgram);
    
    glDeleteVertexArrays(1, &g_quadVAO);
    glDeleteBuffers(1, &g_quadVBO);
    g_quadVAO = 0;
    g_quadVBO = 0;
    
    velocityFree(g_scaler);
    g_scaler = NULL;
}

// ============================================================================
// Frame Operations
// ============================================================================

void resolutionScalerBeginFrame(int* outWidth, int* outHeight) {
    if (!g_scaler || !g_scaler->config.enabled) {
        if (outWidth) *outWidth = g_scaler ? g_scaler->nativeWidth : 0;
        if (outHeight) *outHeight = g_scaler ? g_scaler->nativeHeight : 0;
        return;
    }
    
    // Bind render FBO
    glBindFramebuffer(GL_FRAMEBUFFER, g_scaler->renderFBO);
    glViewport(0, 0, g_scaler->renderWidth, g_scaler->renderHeight);
    
    if (outWidth) *outWidth = g_scaler->renderWidth;
    if (outHeight) *outHeight = g_scaler->renderHeight;
}

void resolutionScalerEndFrame(void) {
    if (!g_scaler || !g_scaler->config.enabled) return;
    
    // Bind default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_scaler->nativeWidth, g_scaler->nativeHeight);
    
    // Disable depth testing for upscale pass
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    // Use upscale shader
    GLuint program = g_scaler->config.sharpening && g_scaler->sharpenProgram ? 
                     g_scaler->sharpenProgram : g_scaler->upscaleProgram;
    
    glUseProgram(program);
    
    // Set uniforms
    if (g_scaler->config.sharpening && g_scaler->sharpenProgram) {
        GLint texelSizeLoc = glGetUniformLocation(program, "uTexelSize");
        GLint sharpnessLoc = glGetUniformLocation(program, "uSharpness");
        
        glUniform2f(texelSizeLoc, 1.0f / g_scaler->nativeWidth, 1.0f / g_scaler->nativeHeight);
        glUniform1f(sharpnessLoc, g_scaler->config.sharpenAmount);
    }
    
    // Bind render texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_scaler->renderColorTex);
    
    // Draw fullscreen quad
    glBindVertexArray(g_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void resolutionScalerRecordFrameTime(float frameTimeMs) {
    if (!g_scaler || !g_scaler->config.enabled) return;
    
    // Add to history
    g_scaler->frameTimeHistory[g_scaler->historyIndex] = frameTimeMs;
    g_scaler->historyIndex = (g_scaler->historyIndex + 1) % SCALER_HISTORY_SIZE;
    
    // Calculate average
    float sum = 0;
    for (int i = 0; i < SCALER_HISTORY_SIZE; i++) {
        sum += g_scaler->frameTimeHistory[i];
    }
    g_scaler->avgFrameTime = sum / SCALER_HISTORY_SIZE;
    g_scaler->actualFPS = 1000.0f / g_scaler->avgFrameTime;
    
    // Adaptive scaling
    float deviation = (g_scaler->avgFrameTime - g_scaler->targetFrameTime) / g_scaler->targetFrameTime;
    
    if (fabsf(deviation) > SCALER_ADJUST_THRESHOLD) {
        float adjustment = -deviation * g_scaler->config.adjustSpeed;
        float newScale = g_scaler->currentScale + adjustment;
        
        // Clamp
        if (newScale < g_scaler->config.minScale) newScale = g_scaler->config.minScale;
        if (newScale > g_scaler->config.maxScale) newScale = g_scaler->config.maxScale;
        
        if (fabsf(newScale - g_scaler->currentScale) > 0.01f) {
            g_scaler->currentScale = newScale;
            updateRenderSize();
        }
    }
}

// ============================================================================
// Getters/Setters
// ============================================================================

void resolutionScalerSetScale(float scale) {
    if (!g_scaler) return;
    
    if (scale < SCALER_MIN_SCALE) scale = SCALER_MIN_SCALE;
    if (scale > SCALER_MAX_SCALE) scale = SCALER_MAX_SCALE;
    
    g_scaler->currentScale = scale;
    updateRenderSize();
}

float resolutionScalerGetScale(void) {
    return g_scaler ? g_scaler->currentScale : 1.0f;
}

void resolutionScalerGetRenderSize(int* width, int* height) {
    if (width) *width = g_scaler ? g_scaler->renderWidth : 0;
    if (height) *height = g_scaler ? g_scaler->renderHeight : 0;
}

void resolutionScalerGetNativeSize(int* width, int* height) {
    if (width) *width = g_scaler ? g_scaler->nativeWidth : 0;
    if (height) *height = g_scaler ? g_scaler->nativeHeight : 0;
}

void resolutionScalerSetEnabled(bool enabled) {
    if (g_scaler) g_scaler->config.enabled = enabled;
}

bool resolutionScalerIsEnabled(void) {
    return g_scaler && g_scaler->config.enabled;
}

void resolutionScalerResize(int nativeWidth, int nativeHeight) {
    if (!g_scaler) return;
    
    g_scaler->nativeWidth = nativeWidth;
    g_scaler->nativeHeight = nativeHeight;
    updateRenderSize();
}

void resolutionScalerSetConfig(const ScalerConfig* config) {
    if (!g_scaler || !config) return;
    memcpy(&g_scaler->config, config, sizeof(ScalerConfig));
    g_scaler->targetFrameTime = 1000.0f / g_scaler->config.targetFPS;
}

ScalerConfig resolutionScalerGetConfig(void) {
    ScalerConfig config = {0};
    if (g_scaler) config = g_scaler->config;
    return config;
}

float resolutionScalerGetActualFPS(void) {
    return g_scaler ? g_scaler->actualFPS : 0.0f;
}

uint32_t resolutionScalerGetScaleChanges(void) {
    return g_scaler ? g_scaler->scaleChanges : 0;
}

void resolutionScalerSetUpscaleMethod(UpscaleMethod method) {
    if (g_scaler) g_scaler->config.upscaleMethod = method;
}

UpscaleMethod resolutionScalerGetUpscaleMethod(void) {
    return g_scaler ? g_scaler->config.upscaleMethod : UPSCALE_BILINEAR;
}

void resolutionScalerSetSharpening(bool enabled, float amount) {
    if (g_scaler) {
        g_scaler->config.sharpening = enabled;
        g_scaler->config.sharpenAmount = amount;
    }
}
