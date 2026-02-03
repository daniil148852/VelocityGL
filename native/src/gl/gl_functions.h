/**
 * GL Functions - OpenGL 4.x API implementation over OpenGL ES 3.x
 * Main wrapper functions that intercept GL calls
 */

#ifndef GL_FUNCTIONS_H
#define GL_FUNCTIONS_H

#include <GLES3/gl32.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Function pointer types for GL functions we wrap
// ============================================================================

// Draw functions
typedef void (*PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (*PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void (*PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (*PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
typedef void (*PFNGLMULTIDRAWARRAYSPROC)(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount);
typedef void (*PFNGLMULTIDRAWELEMENTSPROC)(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount);

// Shader functions
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (*PFNGLGETPROGRAMBINARYPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary);
typedef void (*PFNGLPROGRAMBINARYPROC)(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);

// Texture functions  
typedef void (*PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (*PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (*PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void (*PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum texture);

// Buffer functions
typedef void (*PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void* (*PFNGLMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (*PFNGLUNMAPBUFFERPROC)(GLenum target);

// Framebuffer functions
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);

// State functions
typedef void (*PFNGLENABLEPROC)(GLenum cap);
typedef void (*PFNGLDISABLEPROC)(GLenum cap);
typedef void (*PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (*PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (*PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (*PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);

// ============================================================================
// Wrapped GL Functions (called by the application)
// ============================================================================

// These are the functions we export that replace standard GL calls

// Draw calls
void vglDrawArrays(GLenum mode, GLint first, GLsizei count);
void vglDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void vglDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
void vglDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
void vglMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount);
void vglMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount);
void vglDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices);

// Shader operations
GLuint vglCreateShader(GLenum type);
void vglShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
void vglCompileShader(GLuint shader);
void vglDeleteShader(GLuint shader);
GLuint vglCreateProgram(void);
void vglAttachShader(GLuint program, GLuint shader);
void vglDetachShader(GLuint program, GLuint shader);
void vglLinkProgram(GLuint program);
void vglUseProgram(GLuint program);
void vglDeleteProgram(GLuint program);
void vglGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary);
void vglProgramBinary(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);

// Uniforms
void vglUniform1i(GLint location, GLint v0);
void vglUniform1f(GLint location, GLfloat v0);
void vglUniform2f(GLint location, GLfloat v0, GLfloat v1);
void vglUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void vglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void vglUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

// Texture operations
void vglBindTexture(GLenum target, GLuint texture);
void vglTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
void vglTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
void vglTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
void vglGenerateMipmap(GLenum target);
void vglActiveTexture(GLenum texture);
void vglTexParameteri(GLenum target, GLenum pname, GLint param);
void vglTexParameterf(GLenum target, GLenum pname, GLfloat param);

// Buffer operations
void vglBindBuffer(GLenum target, GLuint buffer);
void vglBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
void vglBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
void* vglMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLboolean vglUnmapBuffer(GLenum target);
void vglBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void vglBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);

// VAO operations
void vglBindVertexArray(GLuint array);
void vglGenVertexArrays(GLsizei n, GLuint* arrays);
void vglDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void vglEnableVertexAttribArray(GLuint index);
void vglDisableVertexAttribArray(GLuint index);
void vglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void vglVertexAttribDivisor(GLuint index, GLuint divisor);

// Framebuffer operations
void vglBindFramebuffer(GLenum target, GLuint framebuffer);
void vglFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void vglFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GLenum vglCheckFramebufferStatus(GLenum target);
void vglDrawBuffers(GLsizei n, const GLenum* bufs);
void vglReadBuffer(GLenum mode);
void vglBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
void vglInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments);

// State management
void vglEnable(GLenum cap);
void vglDisable(GLenum cap);
GLboolean vglIsEnabled(GLenum cap);
void vglBlendFunc(GLenum sfactor, GLenum dfactor);
void vglBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void vglBlendEquation(GLenum mode);
void vglBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void vglDepthFunc(GLenum func);
void vglDepthMask(GLboolean flag);
void vglDepthRangef(GLfloat n, GLfloat f);
void vglCullFace(GLenum mode);
void vglFrontFace(GLenum mode);
void vglPolygonOffset(GLfloat factor, GLfloat units);
void vglLineWidth(GLfloat width);
void vglViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void vglScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void vglColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void vglStencilFunc(GLenum func, GLint ref, GLuint mask);
void vglStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
void vglStencilMask(GLuint mask);

// Clear operations
void vglClear(GLbitfield mask);
void vglClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void vglClearDepthf(GLfloat d);
void vglClearStencil(GLint s);

// Query operations
void vglGetIntegerv(GLenum pname, GLint* data);
void vglGetFloatv(GLenum pname, GLfloat* data);
void vglGetBooleanv(GLenum pname, GLboolean* data);
const GLubyte* vglGetString(GLenum name);
const GLubyte* vglGetStringi(GLenum name, GLuint index);
GLenum vglGetError(void);

// Sync operations
GLsync vglFenceSync(GLenum condition, GLbitfield flags);
void vglDeleteSync(GLsync sync);
GLenum vglClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void vglWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);

// Compute (if available)
void vglDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
void vglMemoryBarrier(GLbitfield barriers);

// ============================================================================
// Function Registration
// ============================================================================

/**
 * Initialize GL function wrappers
 */
bool glFunctionsInit(void);

/**
 * Shutdown GL function wrappers
 */
void glFunctionsShutdown(void);

/**
 * Get function pointer by name (for GetProcAddress)
 */
void* glFunctionsGetProc(const char* name);

#ifdef __cplusplus
}
#endif

#endif // GL_FUNCTIONS_H
