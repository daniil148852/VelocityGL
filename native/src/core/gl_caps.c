/**
 * GL Capabilities Query
 */

#include "gl_wrapper.h"
#include "../utils/log.h"

#include <GLES3/gl32.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Forward declarations
// ============================================================================

bool glExtensionSupported(const char* extension);

// ============================================================================
// Capability Query
// ============================================================================

void glCapsQuery(VelocityGPUCaps* caps) {
    if (!caps) return;
    
    memset(caps, 0, sizeof(VelocityGPUCaps));
    
    // Get strings
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    
    if (vendor) strncpy(caps->vendorString, vendor, sizeof(caps->vendorString) - 1);
    if (renderer) strncpy(caps->rendererString, renderer, sizeof(caps->rendererString) - 1);
    if (version) strncpy(caps->versionString, version, sizeof(caps->versionString) - 1);
    
    // Parse GL ES version
    if (version) {
        if (sscanf(version, "OpenGL ES %d.%d", &caps->glesVersionMajor, &caps->glesVersionMinor) != 2) {
            caps->glesVersionMajor = 3;
            caps->glesVersionMinor = 0;
        }
    }
    
    // Determine emulated GL version
    if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 2) {
        caps->glVersionMajor = 4;
        caps->glVersionMinor = 6;
    } else if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1) {
        caps->glVersionMajor = 4;
        caps->glVersionMinor = 3;
    } else {
        caps->glVersionMajor = 3;
        caps->glVersionMinor = 3;
    }
    
    // Query limits
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->maxTextureSize);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &caps->maxTextureUnits);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps->maxVertexAttribs);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &caps->maxUniformBufferBindings);
    
    // SSBO (ES 3.1+)
    if (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1) {
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &caps->maxShaderStorageBufferBindings);
        
        // Compute shader limits
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &caps->maxComputeWorkGroupSize[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &caps->maxComputeWorkGroupSize[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &caps->maxComputeWorkGroupSize[2]);
    }
    
    // Feature detection
    caps->hasComputeShaders = (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 1);
    caps->hasGeometryShaders = glExtensionSupported("GL_EXT_geometry_shader") ||
                                (caps->glesVersionMajor >= 3 && caps->glesVersionMinor >= 2);
    caps->hasTessellation = glExtensionSupported("GL_EXT_tessellation_shader");
    
    // Anisotropic filtering
    caps->hasAnisotropicFiltering = glExtensionSupported("GL_EXT_texture_filter_anisotropic");
    if (caps->hasAnisotropicFiltering) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &caps->maxAnisotropy);
    }
    
    // Binary shader support
    GLint numBinaryFormats = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numBinaryFormats);
    caps->hasShaderBinaryFormats = (numBinaryFormats > 0);
    
    // Log capabilities
    velocityLogInfo("GL Capabilities:");
    velocityLogInfo("  Max Texture Size: %d", caps->maxTextureSize);
    velocityLogInfo("  Max Texture Units: %d", caps->maxTextureUnits);
    velocityLogInfo("  Max Vertex Attribs: %d", caps->maxVertexAttribs);
    velocityLogInfo("  Compute Shaders: %s", caps->hasComputeShaders ? "yes" : "no");
    velocityLogInfo("  Geometry Shaders: %s", caps->hasGeometryShaders ? "yes" : "no");
    velocityLogInfo("  Anisotropic: %s (max %.1f)", caps->hasAnisotropicFiltering ? "yes" : "no", caps->maxAnisotropy);
}

// ============================================================================
// Limit Queries
// ============================================================================

int glCapsGetMaxTextureSize(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    return value;
}

int glCapsGetMaxTextureUnits(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
    return value;
}

int glCapsGetMaxVertexAttribs(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
    return value;
}

int glCapsGetMaxUniformBufferBindings(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
    return value;
}

int glCapsGetMaxDrawBuffers(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &value);
    return value;
}

int glCapsGetMaxColorAttachments(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
    return value;
}

int glCapsGetMaxSamples(void) {
    GLint value = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &value);
    return value;
}
