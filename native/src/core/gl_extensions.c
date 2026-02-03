/**
 * GL Extensions Detection and Management
 */

#include "gl_wrapper.h"
#include "../utils/log.h"

#include <GLES3/gl32.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Extension Cache
// ============================================================================

static char* g_extensionString = NULL;
static char** g_extensionList = NULL;
static int g_extensionCount = 0;
static bool g_extensionsLoaded = false;

// ============================================================================
// Extension Loading
// ============================================================================

void glExtensionsLoad(void) {
    if (g_extensionsLoaded) return;
    
    // Get extension string
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!extensions) {
        // Try indexed approach for GL ES 3.0+
        GLint numExtensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        
        if (numExtensions > 0) {
            // Build extension string from indexed queries
            size_t totalLen = 0;
            for (GLint i = 0; i < numExtensions; i++) {
                const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
                if (ext) totalLen += strlen(ext) + 1;
            }
            
            g_extensionString = (char*)malloc(totalLen + 1);
            if (g_extensionString) {
                g_extensionString[0] = '\0';
                
                for (GLint i = 0; i < numExtensions; i++) {
                    const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
                    if (ext) {
                        strcat(g_extensionString, ext);
                        strcat(g_extensionString, " ");
                    }
                }
            }
            
            g_extensionCount = numExtensions;
        }
    } else {
        g_extensionString = strdup(extensions);
        
        // Count extensions
        if (g_extensionString) {
            g_extensionCount = 0;
            const char* p = g_extensionString;
            while (*p) {
                while (*p == ' ') p++;
                if (*p) {
                    g_extensionCount++;
                    while (*p && *p != ' ') p++;
                }
            }
        }
    }
    
    // Build extension list
    if (g_extensionCount > 0 && g_extensionString) {
        g_extensionList = (char**)malloc(g_extensionCount * sizeof(char*));
        
        if (g_extensionList) {
            char* str = strdup(g_extensionString);
            if (str) {
                char* token = strtok(str, " ");
                int i = 0;
                
                while (token && i < g_extensionCount) {
                    g_extensionList[i++] = strdup(token);
                    token = strtok(NULL, " ");
                }
                
                free(str);
            }
        }
    }
    
    g_extensionsLoaded = true;
    
    velocityLogInfo("Loaded %d GL extensions", g_extensionCount);
}

void glExtensionsUnload(void) {
    if (g_extensionString) {
        free(g_extensionString);
        g_extensionString = NULL;
    }
    
    if (g_extensionList) {
        for (int i = 0; i < g_extensionCount; i++) {
            if (g_extensionList[i]) {
                free(g_extensionList[i]);
            }
        }
        free(g_extensionList);
        g_extensionList = NULL;
    }
    
    g_extensionCount = 0;
    g_extensionsLoaded = false;
}

// ============================================================================
// Extension Query
// ============================================================================

bool glExtensionSupported(const char* extension) {
    if (!extension) return false;
    
    // Load extensions if not already loaded
    if (!g_extensionsLoaded) {
        glExtensionsLoad();
    }
    
    // Check in list
    if (g_extensionList) {
        for (int i = 0; i < g_extensionCount; i++) {
            if (g_extensionList[i] && strcmp(g_extensionList[i], extension) == 0) {
                return true;
            }
        }
    }
    
    // Fallback: search in string
    if (g_extensionString) {
        size_t extLen = strlen(extension);
        const char* p = g_extensionString;
        
        while ((p = strstr(p, extension)) != NULL) {
            char before = (p == g_extensionString) ? ' ' : *(p - 1);
            char after = p[extLen];
            
            if ((before == ' ' || before == '\0') && 
                (after == ' ' || after == '\0')) {
                return true;
            }
            p += extLen;
        }
    }
    
    return false;
}

int glExtensionCount(void) {
    return g_extensionCount;
}

const char* glExtensionGet(int index) {
    if (index < 0 || index >= g_extensionCount || !g_extensionList) return NULL;
    return g_extensionList[index];
}

const char* glExtensionString(void) {
    return g_extensionString;
}

// ============================================================================
// Common Extension Checks
// ============================================================================

bool glExtensionHasTextureFilterAnisotropic(void) {
    return glExtensionSupported("GL_EXT_texture_filter_anisotropic");
}

bool glExtensionHasDebugOutput(void) {
    return glExtensionSupported("GL_KHR_debug");
}

bool glExtensionHasBufferStorage(void) {
    return glExtensionSupported("GL_EXT_buffer_storage");
}

bool glExtensionHasShaderFramebufferFetch(void) {
    return glExtensionSupported("GL_EXT_shader_framebuffer_fetch") ||
           glExtensionSupported("GL_ARM_shader_framebuffer_fetch");
}

bool glExtensionHasTextureCompressionASTC(void) {
    return glExtensionSupported("GL_KHR_texture_compression_astc_ldr");
}

bool glExtensionHasGeometryShader(void) {
    return glExtensionSupported("GL_EXT_geometry_shader");
}

bool glExtensionHasTessellationShader(void) {
    return glExtensionSupported("GL_EXT_tessellation_shader");
}
