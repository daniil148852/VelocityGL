/**
 * GL Functions - Implementation
 */

#include "gl_functions.h"
#include "../core/gl_wrapper.h"
#include "../buffer/draw_batcher.h"
#include "../shader/shader_cache.h"
#include "../texture/texture_manager.h"
#include "../utils/log.h"

#include <string.h>
#include <stdlib.h>

// ============================================================================
// Function Table for GetProcAddress
// ============================================================================

typedef struct GLFunctionEntry {
    const char* name;
    void* func;
} GLFunctionEntry;

// Forward declarations
static void registerFunctions(void);

static GLFunctionEntry* g_functionTable = NULL;
static int g_functionCount = 0;
static int g_functionCapacity = 0;

static void addFunction(const char* name, void* func) {
    if (g_functionCount >= g_functionCapacity) {
        g_functionCapacity = g_functionCapacity ? g_functionCapacity * 2 : 256;
        g_functionTable = realloc(g_functionTable, g_functionCapacity * sizeof(GLFunctionEntry));
    }
    
    g_functionTable[g_functionCount].name = name;
    g_functionTable[g_functionCount].func = func;
    g_functionCount++;
}

// ============================================================================
// Initialization
// ============================================================================

bool glFunctionsInit(void) {
    velocityLogInfo("Initializing GL function wrappers");
    registerFunctions();
    velocityLogInfo("Registered %d GL functions", g_functionCount);
    return true;
}

void glFunctionsShutdown(void) {
    if (g_functionTable) {
        free(g_functionTable);
        g_functionTable = NULL;
        g_functionCount = 0;
        g_functionCapacity = 0;
    }
}

void* glFunctionsGetProc(const char* name) {
    if (!name) return NULL;
    
    // Binary search would be better, but linear is fine for now
    for (int i = 0; i < g_functionCount; i++) {
        if (strcmp(g_functionTable[i].name, name) == 0) {
            return g_functionTable[i].func;
        }
    }
    
    // Fall back to native
    return (void*)eglGetProcAddress(name);
}

// ============================================================================
// Draw Calls
// ============================================================================

void vglDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (g_wrapperCtx && g_wrapperCtx->config.enableDrawBatching) {
        drawBatcherDrawArrays(mode, first, count);
    } else {
        glDrawArrays(mode, first, count);
        if (g_wrapperCtx) {
            g_wrapperCtx->stats.drawCalls++;
            g_wrapperCtx->stats.triangles += count / 3;
        }
    }
}

void vglDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    if (g_wrapperCtx && g_wrapperCtx->config.enableDrawBatching) {
        drawBatcherDrawElements(mode, count, type, indices);
    } else {
        glDrawElements(mode, count, type, indices);
        if (g_wrapperCtx) {
            g_wrapperCtx->stats.drawCalls++;
            g_wrapperCtx->stats.triangles += count / 3;
        }
    }
}

void vglDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    if (g_wrapperCtx && g_wrapperCtx->config.enableDrawBatching) {
        drawBatcherDrawArraysInstanced(mode, first, count, instancecount);
    } else {
        glDrawArraysInstanced(mode, first, count, instancecount);
        if (g_wrapperCtx) {
            g_wrapperCtx->stats.drawCalls++;
            g_wrapperCtx->stats.triangles += (count / 3) * instancecount;
        }
    }
}

void vglDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, 
                               const void* indices, GLsizei instancecount) {
    glDrawElementsInstanced(mode, count, type, indices, instancecount);
    if (g_wrapperCtx) {
        g_wrapperCtx->stats.drawCalls++;
        g_wrapperCtx->stats.triangles += (count / 3) * instancecount;
    }
}

void vglMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    // OpenGL ES doesn't have glMultiDrawArrays, emulate it
    for (GLsizei i = 0; i < drawcount; i++) {
        glDrawArrays(mode, first[i], count[i]);
    }
    if (g_wrapperCtx) {
        g_wrapperCtx->stats.drawCalls += drawcount;
    }
}

void vglMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, 
                           const void* const* indices, GLsizei drawcount) {
    // OpenGL ES doesn't have glMultiDrawElements, emulate it
    for (GLsizei i = 0; i < drawcount; i++) {
        glDrawElements(mode, count[i], type, indices[i]);
    }
    if (g_wrapperCtx) {
        g_wrapperCtx->stats.drawCalls += drawcount;
    }
}

void vglDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, 
                           GLenum type, const void* indices) {
    // OpenGL ES 3.0 has glDrawRangeElements
    glDrawRangeElements(mode, start, end, count, type, indices);
    if (g_wrapperCtx) {
        g_wrapperCtx->stats.drawCalls++;
        g_wrapperCtx->stats.triangles += count / 3;
    }
}

// ============================================================================
// Shader Operations
// ============================================================================

GLuint vglCreateShader(GLenum type) {
    return glCreateShader(type);
}

void vglShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    // Could translate GLSL here if needed
    glShaderSource(shader, count, string, length);
}

void vglCompileShader(GLuint shader) {
    glCompileShader(shader);
    
    // Check for errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        velocityLogError("Shader compilation failed: %s", log);
    }
}

void vglDeleteShader(GLuint shader) {
    glDeleteShader(shader);
}

GLuint vglCreateProgram(void) {
    return glCreateProgram();
}

void vglAttachShader(GLuint program, GLuint shader) {
    glAttachShader(program, shader);
}

void vglDetachShader(GLuint program, GLuint shader) {
    glDetachShader(program, shader);
}

void vglLinkProgram(GLuint program) {
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        velocityLogError("Program linking failed: %s", log);
    }
}

void vglUseProgram(GLuint program) {
    // Track state
    if (g_wrapperCtx) {
        g_wrapperCtx->state.currentProgram = program;
    }
    glUseProgram(program);
}

void vglDeleteProgram(GLuint program) {
    glDeleteProgram(program);
}

void vglGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, 
                          GLenum* binaryFormat, void* binary) {
    glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void vglProgramBinary(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length) {
    glProgramBinary(program, binaryFormat, binary, length);
}

// ============================================================================
// Uniforms
// ============================================================================

void vglUniform1i(GLint location, GLint v0) {
    glUniform1i(location, v0);
}

void vglUniform1f(GLint location, GLfloat v0) {
    glUniform1f(location, v0);
}

void vglUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    glUniform2f(location, v0, v1);
}

void vglUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    glUniform3f(location, v0, v1, v2);
}

void vglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    glUniform4f(location, v0, v1, v2, v3);
}

void vglUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    glUniformMatrix4fv(location, count, transpose, value);
}

// ============================================================================
// Textures
// ============================================================================

void vglBindTexture(GLenum target, GLuint texture) {
    // Track state
    if (g_wrapperCtx) {
        int unit = g_wrapperCtx->state.activeTextureUnit;
        switch (target) {
            case GL_TEXTURE_2D:
                g_wrapperCtx->state.textureUnits[unit].texture2D = texture;
                break;
            case GL_TEXTURE_3D:
                g_wrapperCtx->state.textureUnits[unit].texture3D = texture;
                break;
            case GL_TEXTURE_CUBE_MAP:
                g_wrapperCtx->state.textureUnits[unit].textureCube = texture;
                break;
        }
    }
    glBindTexture(target, texture);
}

void vglTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, 
                    GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    // Translate unsupported formats
    GLenum esInternalFormat = internalformat;
    GLenum esFormat = format;
    
    // Handle desktop GL formats not in ES
    switch (internalformat) {
        case GL_RGB:
            esInternalFormat = GL_RGB8;
            break;
        case GL_RGBA:
            esInternalFormat = GL_RGBA8;
            break;
        case 0x1903:  // GL_RED (desktop)
            esInternalFormat = GL_R8;
            esFormat = GL_RED;
            break;
    }
    
    glTexImage2D(target, level, esInternalFormat, width, height, border, esFormat, type, pixels);
}

void vglTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
                       GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void vglTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, 
                    GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, 
                    const void* pixels) {
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void vglGenerateMipmap(GLenum target) {
    glGenerateMipmap(target);
}

void vglActiveTexture(GLenum texture) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.activeTextureUnit = texture - GL_TEXTURE0;
    }
    glActiveTexture(texture);
}

void vglTexParameteri(GLenum target, GLenum pname, GLint param) {
    glTexParameteri(target, pname, param);
}

void vglTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    glTexParameterf(target, pname, param);
}

// ============================================================================
// Buffers
// ============================================================================

void vglBindBuffer(GLenum target, GLuint buffer) {
    // Track state
    if (g_wrapperCtx) {
        switch (target) {
            case GL_ARRAY_BUFFER:
                g_wrapperCtx->state.buffers.arrayBuffer = buffer;
                break;
            case GL_ELEMENT_ARRAY_BUFFER:
                g_wrapperCtx->state.buffers.elementBuffer = buffer;
                break;
            case GL_UNIFORM_BUFFER:
                g_wrapperCtx->state.buffers.uniformBuffer = buffer;
                break;
        }
    }
    glBindBuffer(target, buffer);
}

void vglBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    glBufferData(target, size, data, usage);
}

void vglBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    glBufferSubData(target, offset, size, data);
}

void* vglMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    return glMapBufferRange(target, offset, length, access);
}

GLboolean vglUnmapBuffer(GLenum target) {
    return glUnmapBuffer(target);
}

void vglBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    glBindBufferBase(target, index, buffer);
}

void vglBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    glBindBufferRange(target, index, buffer, offset, size);
}

// ============================================================================
// VAO
// ============================================================================

void vglBindVertexArray(GLuint array) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.vertexArray = array;
    }
    glBindVertexArray(array);
}

void vglGenVertexArrays(GLsizei n, GLuint* arrays) {
    glGenVertexArrays(n, arrays);
}

void vglDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    glDeleteVertexArrays(n, arrays);
}

void vglEnableVertexAttribArray(GLuint index) {
    glEnableVertexAttribArray(index);
}

void vglDisableVertexAttribArray(GLuint index) {
    glDisableVertexAttribArray(index);
}

void vglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, 
                             GLsizei stride, const void* pointer) {
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void vglVertexAttribDivisor(GLuint index, GLuint divisor) {
    glVertexAttribDivisor(index, divisor);
}

// ============================================================================
// Framebuffers
// ============================================================================

void vglBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (g_wrapperCtx) {
        if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
            g_wrapperCtx->state.framebuffer.drawFramebuffer = framebuffer;
        }
        if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) {
            g_wrapperCtx->state.framebuffer.readFramebuffer = framebuffer;
        }
    }
    glBindFramebuffer(target, framebuffer);
}

void vglFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, 
                              GLuint texture, GLint level) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void vglFramebufferRenderbuffer(GLenum target, GLenum attachment, 
                                 GLenum renderbuffertarget, GLuint renderbuffer) {
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

GLenum vglCheckFramebufferStatus(GLenum target) {
    return glCheckFramebufferStatus(target);
}

void vglDrawBuffers(GLsizei n, const GLenum* bufs) {
    glDrawBuffers(n, bufs);
}

void vglReadBuffer(GLenum mode) {
    glReadBuffer(mode);
}

void vglBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask, GLenum filter) {
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void vglInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
    glInvalidateFramebuffer(target, numAttachments, attachments);
}

// ============================================================================
// State Management
// ============================================================================

void vglEnable(GLenum cap) {
    // Track common states
    if (g_wrapperCtx) {
        switch (cap) {
            case GL_BLEND:
                g_wrapperCtx->state.blend.enabled = true;
                break;
            case GL_DEPTH_TEST:
                g_wrapperCtx->state.depth.testEnabled = true;
                break;
            case GL_CULL_FACE:
                g_wrapperCtx->state.rasterizer.cullFaceEnabled = true;
                break;
            case GL_SCISSOR_TEST:
                g_wrapperCtx->state.rasterizer.scissorEnabled = true;
                break;
        }
    }
    glEnable(cap);
}

void vglDisable(GLenum cap) {
    if (g_wrapperCtx) {
        switch (cap) {
            case GL_BLEND:
                g_wrapperCtx->state.blend.enabled = false;
                break;
            case GL_DEPTH_TEST:
                g_wrapperCtx->state.depth.testEnabled = false;
                break;
            case GL_CULL_FACE:
                g_wrapperCtx->state.rasterizer.cullFaceEnabled = false;
                break;
            case GL_SCISSOR_TEST:
                g_wrapperCtx->state.rasterizer.scissorEnabled = false;
                break;
        }
    }
    glDisable(cap);
}

GLboolean vglIsEnabled(GLenum cap) {
    return glIsEnabled(cap);
}

void vglBlendFunc(GLenum sfactor, GLenum dfactor) {
    vglBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

void vglBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, 
                           GLenum sfactorAlpha, GLenum dfactorAlpha) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.blend.srcRGB = sfactorRGB;
        g_wrapperCtx->state.blend.dstRGB = dfactorRGB;
        g_wrapperCtx->state.blend.srcAlpha = sfactorAlpha;
        g_wrapperCtx->state.blend.dstAlpha = dfactorAlpha;
    }
    glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void vglBlendEquation(GLenum mode) {
    vglBlendEquationSeparate(mode, mode);
}

void vglBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.blend.modeRGB = modeRGB;
        g_wrapperCtx->state.blend.modeAlpha = modeAlpha;
    }
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void vglDepthFunc(GLenum func) {
    if (g_wrapperCtx) g_wrapperCtx->state.depth.func = func;
    glDepthFunc(func);
}

void vglDepthMask(GLboolean flag) {
    if (g_wrapperCtx) g_wrapperCtx->state.depth.writeEnabled = flag;
    glDepthMask(flag);
}

void vglDepthRangef(GLfloat n, GLfloat f) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.depth.rangeNear = n;
        g_wrapperCtx->state.depth.rangeFar = f;
    }
    glDepthRangef(n, f);
}

void vglCullFace(GLenum mode) {
    if (g_wrapperCtx) g_wrapperCtx->state.rasterizer.cullMode = mode;
    glCullFace(mode);
}

void vglFrontFace(GLenum mode) {
    if (g_wrapperCtx) g_wrapperCtx->state.rasterizer.frontFace = mode;
    glFrontFace(mode);
}

void vglPolygonOffset(GLfloat factor, GLfloat units) {
    glPolygonOffset(factor, units);
}

void vglLineWidth(GLfloat width) {
    if (g_wrapperCtx) g_wrapperCtx->state.rasterizer.lineWidth = width;
    glLineWidth(width);
}

void vglViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.rasterizer.viewport[0] = x;
        g_wrapperCtx->state.rasterizer.viewport[1] = y;
        g_wrapperCtx->state.rasterizer.viewport[2] = width;
        g_wrapperCtx->state.rasterizer.viewport[3] = height;
    }
    glViewport(x, y, width, height);
}

void vglScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.rasterizer.scissor[0] = x;
        g_wrapperCtx->state.rasterizer.scissor[1] = y;
        g_wrapperCtx->state.rasterizer.scissor[2] = width;
        g_wrapperCtx->state.rasterizer.scissor[3] = height;
    }
    glScissor(x, y, width, height);
}

void vglColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    glColorMask(red, green, blue, alpha);
}

void vglStencilFunc(GLenum func, GLint ref, GLuint mask) {
    glStencilFunc(func, ref, mask);
}

void vglStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOp(sfail, dpfail, dppass);
}

void vglStencilMask(GLuint mask) {
    glStencilMask(mask);
}

// ============================================================================
// Clear Operations
// ============================================================================

void vglClear(GLbitfield mask) {
    glClear(mask);
}

void vglClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    if (g_wrapperCtx) {
        g_wrapperCtx->state.clearColor[0] = red;
        g_wrapperCtx->state.clearColor[1] = green;
        g_wrapperCtx->state.clearColor[2] = blue;
        g_wrapperCtx->state.clearColor[3] = alpha;
    }
    glClearColor(red, green, blue, alpha);
}

void vglClearDepthf(GLfloat d) {
    if (g_wrapperCtx) g_wrapperCtx->state.clearDepth = d;
    glClearDepthf(d);
}

void vglClearStencil(GLint s) {
    if (g_wrapperCtx) g_wrapperCtx->state.clearStencil = s;
    glClearStencil(s);
}

// ============================================================================
// Query Operations
// ============================================================================

void vglGetIntegerv(GLenum pname, GLint* data) {
    // Override some values to report GL 4.x
    switch (pname) {
        case GL_MAJOR_VERSION:
            if (g_wrapperCtx) {
                *data = g_wrapperCtx->gpuCaps.glVersionMajor;
                return;
            }
            break;
        case GL_MINOR_VERSION:
            if (g_wrapperCtx) {
                *data = g_wrapperCtx->gpuCaps.glVersionMinor;
                return;
            }
            break;
    }
    glGetIntegerv(pname, data);
}

void vglGetFloatv(GLenum pname, GLfloat* data) {
    glGetFloatv(pname, data);
}

void vglGetBooleanv(GLenum pname, GLboolean* data) {
    glGetBooleanv(pname, data);
}

const GLubyte* vglGetString(GLenum name) {
    // Override version string
    static char versionString[128];
    static char rendererString[256];
    
    switch (name) {
        case GL_VERSION:
            if (g_wrapperCtx) {
                snprintf(versionString, sizeof(versionString), 
                        "%d.%d VelocityGL", 
                        g_wrapperCtx->gpuCaps.glVersionMajor,
                        g_wrapperCtx->gpuCaps.glVersionMinor);
                return (const GLubyte*)versionString;
            }
            break;
            
        case GL_RENDERER:
            if (g_wrapperCtx) {
                snprintf(rendererString, sizeof(rendererString),
                        "VelocityGL (%s)",
                        g_wrapperCtx->gpuCaps.rendererString);
                return (const GLubyte*)rendererString;
            }
            break;
    }
    
    return glGetString(name);
}

const GLubyte* vglGetStringi(GLenum name, GLuint index) {
    return glGetStringi(name, index);
}

GLenum vglGetError(void) {
    return glGetError();
}

// ============================================================================
// Sync
// ============================================================================

GLsync vglFenceSync(GLenum condition, GLbitfield flags) {
    return glFenceSync(condition, flags);
}

void vglDeleteSync(GLsync sync) {
    glDeleteSync(sync);
}

GLenum vglClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    return glClientWaitSync(sync, flags, timeout);
}

void vglWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    glWaitSync(sync, flags, timeout);
}

// ============================================================================
// Compute
// ============================================================================

void vglDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

void vglMemoryBarrier(GLbitfield barriers) {
    glMemoryBarrier(barriers);
}

// ============================================================================
// Function Registration
// ============================================================================

static void registerFunctions(void) {
    // Draw calls
    addFunction("glDrawArrays", vglDrawArrays);
    addFunction("glDrawElements", vglDrawElements);
    addFunction("glDrawArraysInstanced", vglDrawArraysInstanced);
    addFunction("glDrawElementsInstanced", vglDrawElementsInstanced);
    addFunction("glMultiDrawArrays", vglMultiDrawArrays);
    addFunction("glMultiDrawElements", vglMultiDrawElements);
    addFunction("glDrawRangeElements", vglDrawRangeElements);
    
    // Shaders
    addFunction("glCreateShader", vglCreateShader);
    addFunction("glShaderSource", vglShaderSource);
    addFunction("glCompileShader", vglCompileShader);
    addFunction("glDeleteShader", vglDeleteShader);
    addFunction("glCreateProgram", vglCreateProgram);
    addFunction("glAttachShader", vglAttachShader);
    addFunction("glDetachShader", vglDetachShader);
    addFunction("glLinkProgram", vglLinkProgram);
    addFunction("glUseProgram", vglUseProgram);
    addFunction("glDeleteProgram", vglDeleteProgram);
    addFunction("glGetProgramBinary", vglGetProgramBinary);
    addFunction("glProgramBinary", vglProgramBinary);
    
    // Uniforms
    addFunction("glUniform1i", vglUniform1i);
    addFunction("glUniform1f", vglUniform1f);
    addFunction("glUniform2f", vglUniform2f);
    addFunction("glUniform3f", vglUniform3f);
    addFunction("glUniform4f", vglUniform4f);
    addFunction("glUniformMatrix4fv", vglUniformMatrix4fv);
    
    // Textures
    addFunction("glBindTexture", vglBindTexture);
    addFunction("glTexImage2D", vglTexImage2D);
    addFunction("glTexSubImage2D", vglTexSubImage2D);
    addFunction("glTexImage3D", vglTexImage3D);
    addFunction("glGenerateMipmap", vglGenerateMipmap);
    addFunction("glActiveTexture", vglActiveTexture);
    addFunction("glTexParameteri", vglTexParameteri);
    addFunction("glTexParameterf", vglTexParameterf);
    
    // Buffers
    addFunction("glBindBuffer", vglBindBuffer);
    addFunction("glBufferData", vglBufferData);
    addFunction("glBufferSubData", vglBufferSubData);
    addFunction("glMapBufferRange", vglMapBufferRange);
    addFunction("glUnmapBuffer", vglUnmapBuffer);
    addFunction("glBindBufferBase", vglBindBufferBase);
    addFunction("glBindBufferRange", vglBindBufferRange);
    
    // VAO
    addFunction("glBindVertexArray", vglBindVertexArray);
    addFunction("glGenVertexArrays", vglGenVertexArrays);
    addFunction("glDeleteVertexArrays", vglDeleteVertexArrays);
    addFunction("glEnableVertexAttribArray", vglEnableVertexAttribArray);
    addFunction("glDisableVertexAttribArray", vglDisableVertexAttribArray);
    addFunction("glVertexAttribPointer", vglVertexAttribPointer);
    addFunction("glVertexAttribDivisor", vglVertexAttribDivisor);
    
    // Framebuffers
    addFunction("glBindFramebuffer", vglBindFramebuffer);
    addFunction("glFramebufferTexture2D", vglFramebufferTexture2D);
    addFunction("glFramebufferRenderbuffer", vglFramebufferRenderbuffer);
    addFunction("glCheckFramebufferStatus", vglCheckFramebufferStatus);
    addFunction("glDrawBuffers", vglDrawBuffers);
    addFunction("glReadBuffer", vglReadBuffer);
    addFunction("glBlitFramebuffer", vglBlitFramebuffer);
    addFunction("glInvalidateFramebuffer", vglInvalidateFramebuffer);
    
    // State
    addFunction("glEnable", vglEnable);
    addFunction("glDisable", vglDisable);
    addFunction("glIsEnabled", vglIsEnabled);
    addFunction("glBlendFunc", vglBlendFunc);
    addFunction("glBlendFuncSeparate", vglBlendFuncSeparate);
    addFunction("glBlendEquation", vglBlendEquation);
    addFunction("glBlendEquationSeparate", vglBlendEquationSeparate);
    addFunction("glDepthFunc", vglDepthFunc);
    addFunction("glDepthMask", vglDepthMask);
    addFunction("glDepthRangef", vglDepthRangef);
    addFunction("glCullFace", vglCullFace);
    addFunction("glFrontFace", vglFrontFace);
    addFunction("glPolygonOffset", vglPolygonOffset);
    addFunction("glLineWidth", vglLineWidth);
    addFunction("glViewport", vglViewport);
    addFunction("glScissor", vglScissor);
    addFunction("glColorMask", vglColorMask);
    addFunction("glStencilFunc", vglStencilFunc);
    addFunction("glStencilOp", vglStencilOp);
    addFunction("glStencilMask", vglStencilMask);
    
    // Clear
    addFunction("glClear", vglClear);
    addFunction("glClearColor", vglClearColor);
    addFunction("glClearDepthf", vglClearDepthf);
    addFunction("glClearStencil", vglClearStencil);
    
    // Query
    addFunction("glGetIntegerv", vglGetIntegerv);
    addFunction("glGetFloatv", vglGetFloatv);
    addFunction("glGetBooleanv", vglGetBooleanv);
    addFunction("glGetString", vglGetString);
    addFunction("glGetStringi", vglGetStringi);
    addFunction("glGetError", vglGetError);
    
    // Sync
    addFunction("glFenceSync", vglFenceSync);
    addFunction("glDeleteSync", vglDeleteSync);
    addFunction("glClientWaitSync", vglClientWaitSync);
    addFunction("glWaitSync", vglWaitSync);
    
    // Compute
    addFunction("glDispatchCompute", vglDispatchCompute);
    addFunction("glMemoryBarrier", vglMemoryBarrier);
    
    // Additional Gen/Delete functions
    addFunction("glGenTextures", glGenTextures);
    addFunction("glDeleteTextures", glDeleteTextures);
    addFunction("glGenBuffers", glGenBuffers);
    addFunction("glDeleteBuffers", glDeleteBuffers);
    addFunction("glGenFramebuffers", glGenFramebuffers);
    addFunction("glDeleteFramebuffers", glDeleteFramebuffers);
    addFunction("glGenRenderbuffers", glGenRenderbuffers);
    addFunction("glDeleteRenderbuffers", glDeleteRenderbuffers);
    addFunction("glBindRenderbuffer", glBindRenderbuffer);
    addFunction("glRenderbufferStorage", glRenderbufferStorage);
    addFunction("glRenderbufferStorageMultisample", glRenderbufferStorageMultisample);
    
    // Shader queries
    addFunction("glGetShaderiv", glGetShaderiv);
    addFunction("glGetShaderInfoLog", glGetShaderInfoLog);
    addFunction("glGetProgramiv", glGetProgramiv);
    addFunction("glGetProgramInfoLog", glGetProgramInfoLog);
    addFunction("glGetUniformLocation", glGetUniformLocation);
    addFunction("glGetAttribLocation", glGetAttribLocation);
    addFunction("glGetActiveUniform", glGetActiveUniform);
    addFunction("glGetActiveAttrib", glGetActiveAttrib);
    addFunction("glGetUniformBlockIndex", glGetUniformBlockIndex);
    addFunction("glUniformBlockBinding", glUniformBlockBinding);
    
    // More uniforms
    addFunction("glUniform1iv", glUniform1iv);
    addFunction("glUniform2i", glUniform2i);
    addFunction("glUniform2iv", glUniform2iv);
    addFunction("glUniform3i", glUniform3i);
    addFunction("glUniform3iv", glUniform3iv);
    addFunction("glUniform4i", glUniform4i);
    addFunction("glUniform4iv", glUniform4iv);
    addFunction("glUniform1fv", glUniform1fv);
    addFunction("glUniform2fv", glUniform2fv);
    addFunction("glUniform3fv", glUniform3fv);
    addFunction("glUniform4fv", glUniform4fv);
    addFunction("glUniformMatrix2fv", glUniformMatrix2fv);
    addFunction("glUniformMatrix3fv", glUniformMatrix3fv);
    addFunction("glUniformMatrix2x3fv", glUniformMatrix2x3fv);
    addFunction("glUniformMatrix3x2fv", glUniformMatrix3x2fv);
    addFunction("glUniformMatrix2x4fv", glUniformMatrix2x4fv);
    addFunction("glUniformMatrix4x2fv", glUniformMatrix4x2fv);
    addFunction("glUniformMatrix3x4fv", glUniformMatrix3x4fv);
    addFunction("glUniformMatrix4x3fv", glUniformMatrix4x3fv);
    
    // Vertex attributes
    addFunction("glVertexAttrib1f", glVertexAttrib1f);
    addFunction("glVertexAttrib2f", glVertexAttrib2f);
    addFunction("glVertexAttrib3f", glVertexAttrib3f);
    addFunction("glVertexAttrib4f", glVertexAttrib4f);
    addFunction("glVertexAttrib1fv", glVertexAttrib1fv);
    addFunction("glVertexAttrib2fv", glVertexAttrib2fv);
    addFunction("glVertexAttrib3fv", glVertexAttrib3fv);
    addFunction("glVertexAttrib4fv", glVertexAttrib4fv);
    addFunction("glVertexAttribIPointer", glVertexAttribIPointer);
    addFunction("glVertexAttribI4i", glVertexAttribI4i);
    addFunction("glVertexAttribI4ui", glVertexAttribI4ui);
    
    // Texture functions
    addFunction("glTexStorage2D", glTexStorage2D);
    addFunction("glTexStorage3D", glTexStorage3D);
    addFunction("glTexSubImage3D", glTexSubImage3D);
    addFunction("glCompressedTexImage2D", glCompressedTexImage2D);
    addFunction("glCompressedTexImage3D", glCompressedTexImage3D);
    addFunction("glCompressedTexSubImage2D", glCompressedTexSubImage2D);
    addFunction("glCompressedTexSubImage3D", glCompressedTexSubImage3D);
    addFunction("glCopyTexImage2D", glCopyTexImage2D);
    addFunction("glCopyTexSubImage2D", glCopyTexSubImage2D);
    addFunction("glCopyTexSubImage3D", glCopyTexSubImage3D);
    addFunction("glTexParameteriv", glTexParameteriv);
    addFunction("glTexParameterfv", glTexParameterfv);
    addFunction("glGetTexParameteriv", glGetTexParameteriv);
    addFunction("glGetTexParameterfv", glGetTexParameterfv);
    addFunction("glPixelStorei", glPixelStorei);
    
    // Sampler objects
    addFunction("glGenSamplers", glGenSamplers);
    addFunction("glDeleteSamplers", glDeleteSamplers);
    addFunction("glBindSampler", glBindSampler);
    addFunction("glSamplerParameteri", glSamplerParameteri);
    addFunction("glSamplerParameterf", glSamplerParameterf);
    addFunction("glSamplerParameteriv", glSamplerParameteriv);
    addFunction("glSamplerParameterfv", glSamplerParameterfv);
    
    // Read pixels
    addFunction("glReadPixels", glReadPixels);
    
    // Queries
    addFunction("glGenQueries", glGenQueries);
    addFunction("glDeleteQueries", glDeleteQueries);
    addFunction("glBeginQuery", glBeginQuery);
    addFunction("glEndQuery", glEndQuery);
    addFunction("glGetQueryiv", glGetQueryiv);
    addFunction("glGetQueryObjectuiv", glGetQueryObjectuiv);
    
    // Transform feedback
    addFunction("glGenTransformFeedbacks", glGenTransformFeedbacks);
    addFunction("glDeleteTransformFeedbacks", glDeleteTransformFeedbacks);
    addFunction("glBindTransformFeedback", glBindTransformFeedback);
    addFunction("glBeginTransformFeedback", glBeginTransformFeedback);
    addFunction("glEndTransformFeedback", glEndTransformFeedback);
    addFunction("glPauseTransformFeedback", glPauseTransformFeedback);
    addFunction("glResumeTransformFeedback", glResumeTransformFeedback);
    addFunction("glTransformFeedbackVaryings", glTransformFeedbackVaryings);
    addFunction("glGetTransformFeedbackVarying", glGetTransformFeedbackVarying);
    
    // Program pipeline (if supported)
    addFunction("glGenProgramPipelines", glGenProgramPipelines);
    addFunction("glDeleteProgramPipelines", glDeleteProgramPipelines);
    addFunction("glBindProgramPipeline", glBindProgramPipeline);
    addFunction("glUseProgramStages", glUseProgramStages);
    addFunction("glActiveShaderProgram", glActiveShaderProgram);
    addFunction("glProgramUniform1i", glProgramUniform1i);
    addFunction("glProgramUniform1f", glProgramUniform1f);
    addFunction("glProgramUniform4fv", glProgramUniform4fv);
    addFunction("glProgramUniformMatrix4fv", glProgramUniformMatrix4fv);
    
    // Misc
    addFunction("glFlush", glFlush);
    addFunction("glFinish", glFinish);
    addFunction("glHint", glHint);
    addFunction("glIsTexture", glIsTexture);
    addFunction("glIsBuffer", glIsBuffer);
    addFunction("glIsFramebuffer", glIsFramebuffer);
    addFunction("glIsProgram", glIsProgram);
    addFunction("glIsShader", glIsShader);
    addFunction("glIsVertexArray", glIsVertexArray);
    
    // Debug (if available)
    addFunction("glDebugMessageCallback", glDebugMessageCallback);
    addFunction("glDebugMessageControl", glDebugMessageControl);
    addFunction("glDebugMessageInsert", glDebugMessageInsert);
    addFunction("glGetDebugMessageLog", glGetDebugMessageLog);
    addFunction("glPushDebugGroup", glPushDebugGroup);
    addFunction("glPopDebugGroup", glPopDebugGroup);
    addFunction("glObjectLabel", glObjectLabel);
    
    velocityLogInfo("Registered %d GL functions", g_functionCount);
}
