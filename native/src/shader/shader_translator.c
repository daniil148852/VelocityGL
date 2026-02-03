/**
 * GLSL Shader Translator
 * Converts desktop GLSL to GLSL ES
 */

#include "shader_cache.h"
#include "../utils/log.h"
#include "../utils/memory.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Translation Helpers
// ============================================================================

static char* stringReplace(const char* source, const char* find, const char* replace) {
    if (!source || !find || !replace) return NULL;
    
    size_t findLen = strlen(find);
    size_t replaceLen = strlen(replace);
    
    // Count occurrences
    int count = 0;
    const char* p = source;
    while ((p = strstr(p, find)) != NULL) {
        count++;
        p += findLen;
    }
    
    if (count == 0) {
        return velocityStrdup(source);
    }
    
    // Allocate result
    size_t resultLen = strlen(source) + count * (replaceLen - findLen);
    char* result = (char*)velocityMalloc(resultLen + 1);
    
    // Perform replacement
    char* dst = result;
    p = source;
    while (*p) {
        if (strncmp(p, find, findLen) == 0) {
            memcpy(dst, replace, replaceLen);
            dst += replaceLen;
            p += findLen;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    
    return result;
}

// ============================================================================
// Version Directive Handling
// ============================================================================

static bool hasVersionDirective(const char* source) {
    return strstr(source, "#version") != NULL;
}

static int extractVersion(const char* source) {
    const char* version = strstr(source, "#version");
    if (!version) return 0;
    
    version += 8; // Skip "#version"
    while (*version && isspace(*version)) version++;
    
    return atoi(version);
}

static char* replaceVersionDirective(const char* source, const char* newVersion) {
    const char* versionStart = strstr(source, "#version");
    if (!versionStart) {
        // Add version at the beginning
        size_t len = strlen(newVersion) + strlen(source) + 2;
        char* result = (char*)velocityMalloc(len);
        snprintf(result, len, "%s\n%s", newVersion, source);
        return result;
    }
    
    // Find end of version line
    const char* lineEnd = strchr(versionStart, '\n');
    if (!lineEnd) lineEnd = versionStart + strlen(versionStart);
    
    // Build result
    size_t prefixLen = versionStart - source;
    size_t suffixStart = lineEnd - source;
    size_t newVersionLen = strlen(newVersion);
    size_t resultLen = prefixLen + newVersionLen + strlen(source + suffixStart);
    
    char* result = (char*)velocityMalloc(resultLen + 1);
    
    // Copy prefix
    memcpy(result, source, prefixLen);
    // Copy new version
    memcpy(result + prefixLen, newVersion, newVersionLen);
    // Copy suffix (including newline)
    strcpy(result + prefixLen + newVersionLen, source + suffixStart);
    
    return result;
}

// ============================================================================
// Main Translation
// ============================================================================

char* shaderTranslate(const char* source, ShaderType type) {
    if (!source) return NULL;
    
    char* result = velocityStrdup(source);
    char* temp;
    
    // Get source version
    int version = extractVersion(source);
    
    // Replace version directive
    if (version >= 400 || version == 0) {
        temp = replaceVersionDirective(result, "#version 320 es");
        velocityFree(result);
        result = temp;
    } else if (version >= 300 && version < 320) {
        temp = replaceVersionDirective(result, "#version 300 es");
        velocityFree(result);
        result = temp;
    }
    
    // Add precision qualifiers if not present
    if (type == SHADER_TYPE_FRAGMENT) {
        if (!strstr(result, "precision ")) {
            const char* precisionHeader = 
                "precision highp float;\n"
                "precision highp int;\n"
                "precision highp sampler2D;\n"
                "precision highp sampler3D;\n"
                "precision highp samplerCube;\n";
            
            // Find end of version line
            char* versionEnd = strstr(result, "\n");
            if (versionEnd) {
                versionEnd++;
                size_t prefixLen = versionEnd - result;
                size_t headerLen = strlen(precisionHeader);
                size_t suffixLen = strlen(versionEnd);
                
                temp = (char*)velocityMalloc(prefixLen + headerLen + suffixLen + 1);
                memcpy(temp, result, prefixLen);
                memcpy(temp + prefixLen, precisionHeader, headerLen);
                memcpy(temp + prefixLen + headerLen, versionEnd, suffixLen + 1);
                
                velocityFree(result);
                result = temp;
            }
        }
    }
    
    // Replace desktop GL types/functions with ES equivalents
    
    // texture2D -> texture (in GLSL 300+)
    if (strstr(result, "#version 3")) {
        temp = stringReplace(result, "texture2D(", "texture(");
        velocityFree(result);
        result = temp;
        
        temp = stringReplace(result, "texture3D(", "texture(");
        velocityFree(result);
        result = temp;
        
        temp = stringReplace(result, "textureCube(", "texture(");
        velocityFree(result);
        result = temp;
        
        temp = stringReplace(result, "shadow2D(", "texture(");
        velocityFree(result);
        result = temp;
    }
    
    // gl_FragColor -> out variable (GLSL 300+)
    if (type == SHADER_TYPE_FRAGMENT && strstr(result, "gl_FragColor")) {
        // Add output declaration after precision statements
        const char* fragColorDecl = "out vec4 fragColor;\n";
        
        // Find insertion point (after #version and precision)
        char* insertPoint = result;
        char* precisionEnd = strstr(result, "precision");
        if (precisionEnd) {
            // Skip all precision statements
            while ((precisionEnd = strstr(insertPoint, "precision")) != NULL) {
                char* lineEnd = strchr(precisionEnd, '\n');
                if (lineEnd) {
                    insertPoint = lineEnd + 1;
                } else {
                    break;
                }
            }
        } else {
            // Insert after #version
            char* versionEnd = strchr(result, '\n');
            if (versionEnd) {
                insertPoint = versionEnd + 1;
            }
        }
        
        // Insert declaration and replace gl_FragColor
        size_t prefixLen = insertPoint - result;
        size_t declLen = strlen(fragColorDecl);
        size_t suffixLen = strlen(insertPoint);
        
        temp = (char*)velocityMalloc(prefixLen + declLen + suffixLen + 1);
        memcpy(temp, result, prefixLen);
        memcpy(temp + prefixLen, fragColorDecl, declLen);
        memcpy(temp + prefixLen + declLen, insertPoint, suffixLen + 1);
        
        velocityFree(result);
        result = temp;
        
        temp = stringReplace(result, "gl_FragColor", "fragColor");
        velocityFree(result);
        result = temp;
    }
    
    // Replace unsupported functions
    temp = stringReplace(result, "gl_ClipVertex", "// gl_ClipVertex (unsupported)");
    velocityFree(result);
    result = temp;
    
    return result;
}

// ============================================================================
// Shader Optimization
// ============================================================================

char* shaderOptimize(const char* source, ShaderType type) {
    // For now, just return a copy
    // A real implementation would perform optimizations
    return velocityStrdup(source);
}
