/**
 * VelocityGL Configuration System
 * JSON-based configuration loading and saving
 */

#include "config.h"
#include "log.h"
#include "memory.h"
#include "../velocity_gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

// ============================================================================
// Simple JSON Parser (minimal, no external dependencies)
// ============================================================================

typedef enum JsonTokenType {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY_START,
    JSON_ARRAY_END,
    JSON_OBJECT_START,
    JSON_OBJECT_END,
    JSON_COLON,
    JSON_COMMA,
    JSON_EOF,
    JSON_ERROR
} JsonTokenType;

typedef struct JsonToken {
    JsonTokenType type;
    union {
        bool boolValue;
        double numberValue;
        char* stringValue;
    };
} JsonToken;

typedef struct JsonParser {
    const char* data;
    size_t pos;
    size_t length;
    char error[256];
} JsonParser;

// ============================================================================
// Parser Helpers
// ============================================================================

static void skipWhitespace(JsonParser* p) {
    while (p->pos < p->length && isspace(p->data[p->pos])) {
        p->pos++;
    }
}

static JsonToken parseString(JsonParser* p) {
    JsonToken token = {JSON_STRING};
    
    p->pos++;  // Skip opening quote
    
    size_t start = p->pos;
    while (p->pos < p->length && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\') {
            p->pos++;  // Skip escape
        }
        p->pos++;
    }
    
    size_t len = p->pos - start;
    token.stringValue = (char*)velocityMalloc(len + 1);
    
    // Copy with escape handling
    size_t j = 0;
    for (size_t i = start; i < p->pos; i++) {
        if (p->data[i] == '\\' && i + 1 < p->pos) {
            i++;
            switch (p->data[i]) {
                case 'n': token.stringValue[j++] = '\n'; break;
                case 't': token.stringValue[j++] = '\t'; break;
                case 'r': token.stringValue[j++] = '\r'; break;
                case '\\': token.stringValue[j++] = '\\'; break;
                case '"': token.stringValue[j++] = '"'; break;
                default: token.stringValue[j++] = p->data[i]; break;
            }
        } else {
            token.stringValue[j++] = p->data[i];
        }
    }
    token.stringValue[j] = '\0';
    
    p->pos++;  // Skip closing quote
    
    return token;
}

static JsonToken parseNumber(JsonParser* p) {
    JsonToken token = {JSON_NUMBER};
    
    char* end;
    token.numberValue = strtod(p->data + p->pos, &end);
    p->pos = end - p->data;
    
    return token;
}

static JsonToken parseKeyword(JsonParser* p) {
    if (strncmp(p->data + p->pos, "true", 4) == 0) {
        p->pos += 4;
        return (JsonToken){JSON_BOOL, .boolValue = true};
    }
    if (strncmp(p->data + p->pos, "false", 5) == 0) {
        p->pos += 5;
        return (JsonToken){JSON_BOOL, .boolValue = false};
    }
    if (strncmp(p->data + p->pos, "null", 4) == 0) {
        p->pos += 4;
        return (JsonToken){JSON_NULL};
    }
    
    return (JsonToken){JSON_ERROR};
}

static JsonToken nextToken(JsonParser* p) {
    skipWhitespace(p);
    
    if (p->pos >= p->length) {
        return (JsonToken){JSON_EOF};
    }
    
    char c = p->data[p->pos];
    
    switch (c) {
        case '{': p->pos++; return (JsonToken){JSON_OBJECT_START};
        case '}': p->pos++; return (JsonToken){JSON_OBJECT_END};
        case '[': p->pos++; return (JsonToken){JSON_ARRAY_START};
        case ']': p->pos++; return (JsonToken){JSON_ARRAY_END};
        case ':': p->pos++; return (JsonToken){JSON_COLON};
        case ',': p->pos++; return (JsonToken){JSON_COMMA};
        case '"': return parseString(p);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parseNumber(p);
        case 't': case 'f': case 'n':
            return parseKeyword(p);
        default:
            snprintf(p->error, sizeof(p->error), "Unexpected character '%c' at position %zu", c, p->pos);
            return (JsonToken){JSON_ERROR};
    }
}

// ============================================================================
// Configuration File I/O
// ============================================================================

static char* readFile(const char* path, size_t* outSize) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        velocityLogError("Failed to open config file: %s (errno=%d)", path, errno);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(file);
        return NULL;
    }
    
    char* data = (char*)velocityMalloc(size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(data, 1, size, file);
    fclose(file);
    
    if (read != (size_t)size) {
        velocityFree(data);
        return NULL;
    }
    
    data[size] = '\0';
    if (outSize) *outSize = size;
    
    return data;
}

static bool writeFile(const char* path, const char* data, size_t size) {
    // Ensure directory exists
    char* dir = velocityStrdup(path);
    char* lastSlash = strrchr(dir, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        mkdir(dir, 0755);
    }
    velocityFree(dir);
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        velocityLogError("Failed to create config file: %s (errno=%d)", path, errno);
        return false;
    }
    
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    return written == size;
}

// ============================================================================
// Configuration Parsing
// ============================================================================

bool velocityConfigLoad(const char* path, VelocityConfig* config) {
    if (!path || !config) {
        return false;
    }
    
    velocityLogInfo("Loading configuration from: %s", path);
    
    size_t size;
    char* data = readFile(path, &size);
    if (!data) {
        velocityLogWarn("Config file not found, using defaults");
        *config = velocityGetDefaultConfig();
        return false;
    }
    
    // Start with defaults
    *config = velocityGetDefaultConfig();
    
    // Parse JSON
    JsonParser parser = {
        .data = data,
        .pos = 0,
        .length = size
    };
    
    JsonToken token = nextToken(&parser);
    if (token.type != JSON_OBJECT_START) {
        velocityLogError("Config file must start with {");
        velocityFree(data);
        return false;
    }
    
    // Parse key-value pairs
    while (1) {
        token = nextToken(&parser);
        
        if (token.type == JSON_OBJECT_END) {
            break;
        }
        
        if (token.type != JSON_STRING) {
            velocityLogError("Expected string key in config");
            break;
        }
        
        char* key = token.stringValue;
        
        token = nextToken(&parser);
        if (token.type != JSON_COLON) {
            velocityFree(key);
            break;
        }
        
        token = nextToken(&parser);
        
        // Match key and apply value
        if (strcmp(key, "quality") == 0 && token.type == JSON_NUMBER) {
            config->quality = (VelocityQualityPreset)(int)token.numberValue;
        }
        else if (strcmp(key, "backend") == 0 && token.type == JSON_NUMBER) {
            config->backend = (VelocityBackend)(int)token.numberValue;
        }
        else if (strcmp(key, "shaderCache") == 0 && token.type == JSON_NUMBER) {
            config->shaderCache = (VelocityShaderCacheMode)(int)token.numberValue;
        }
        else if (strcmp(key, "shaderCachePath") == 0 && token.type == JSON_STRING) {
            config->shaderCachePath = token.stringValue;
            token.stringValue = NULL;  // Transfer ownership
        }
        else if (strcmp(key, "shaderCacheMaxSize") == 0 && token.type == JSON_NUMBER) {
            config->shaderCacheMaxSize = (size_t)token.numberValue;
        }
        else if (strcmp(key, "enableDynamicResolution") == 0 && token.type == JSON_BOOL) {
            config->enableDynamicResolution = token.boolValue;
        }
        else if (strcmp(key, "minResolutionScale") == 0 && token.type == JSON_NUMBER) {
            config->minResolutionScale = (float)token.numberValue;
        }
        else if (strcmp(key, "maxResolutionScale") == 0 && token.type == JSON_NUMBER) {
            config->maxResolutionScale = (float)token.numberValue;
        }
        else if (strcmp(key, "targetFPS") == 0 && token.type == JSON_NUMBER) {
            config->targetFPS = (int)token.numberValue;
        }
        else if (strcmp(key, "enableDrawBatching") == 0 && token.type == JSON_BOOL) {
            config->enableDrawBatching = token.boolValue;
        }
        else if (strcmp(key, "enableInstancing") == 0 && token.type == JSON_BOOL) {
            config->enableInstancing = token.boolValue;
        }
        else if (strcmp(key, "maxBatchSize") == 0 && token.type == JSON_NUMBER) {
            config->maxBatchSize = (int)token.numberValue;
        }
        else if (strcmp(key, "enableTextureCompression") == 0 && token.type == JSON_BOOL) {
            config->enableTextureCompression = token.boolValue;
        }
        else if (strcmp(key, "texturePoolSize") == 0 && token.type == JSON_NUMBER) {
            config->texturePoolSize = (int)token.numberValue;
        }
        else if (strcmp(key, "maxTextureSize") == 0 && token.type == JSON_NUMBER) {
            config->maxTextureSize = (int)token.numberValue;
        }
        else if (strcmp(key, "enableBufferPooling") == 0 && token.type == JSON_BOOL) {
            config->enableBufferPooling = token.boolValue;
        }
        else if (strcmp(key, "bufferPoolSize") == 0 && token.type == JSON_NUMBER) {
            config->bufferPoolSize = (int)token.numberValue;
        }
        else if (strcmp(key, "enableGPUSpecificTweaks") == 0 && token.type == JSON_BOOL) {
            config->enableGPUSpecificTweaks = token.boolValue;
        }
        else if (strcmp(key, "enableDebugOutput") == 0 && token.type == JSON_BOOL) {
            config->enableDebugOutput = token.boolValue;
        }
        else if (strcmp(key, "enableProfiling") == 0 && token.type == JSON_BOOL) {
            config->enableProfiling = token.boolValue;
        }
        
        velocityFree(key);
        if (token.type == JSON_STRING && token.stringValue) {
            velocityFree(token.stringValue);
        }
        
        // Check for comma
        token = nextToken(&parser);
        if (token.type == JSON_OBJECT_END) {
            break;
        }
        if (token.type != JSON_COMMA) {
            break;
        }
    }
    
    velocityFree(data);
    
    velocityLogInfo("Configuration loaded successfully");
    return true;
}

bool velocityConfigSave(const char* path, const VelocityConfig* config) {
    if (!path || !config) {
        return false;
    }
    
    velocityLogInfo("Saving configuration to: %s", path);
    
    // Build JSON manually (simple approach)
    char buffer[4096];
    int len = snprintf(buffer, sizeof(buffer),
        "{\n"
        "  \"quality\": %d,\n"
        "  \"backend\": %d,\n"
        "  \"shaderCache\": %d,\n"
        "  \"shaderCachePath\": \"%s\",\n"
        "  \"shaderCacheMaxSize\": %zu,\n"
        "  \"enableDynamicResolution\": %s,\n"
        "  \"minResolutionScale\": %.2f,\n"
        "  \"maxResolutionScale\": %.2f,\n"
        "  \"targetFPS\": %d,\n"
        "  \"enableDrawBatching\": %s,\n"
        "  \"enableInstancing\": %s,\n"
        "  \"maxBatchSize\": %d,\n"
        "  \"enableTextureCompression\": %s,\n"
        "  \"texturePoolSize\": %d,\n"
        "  \"maxTextureSize\": %d,\n"
        "  \"enableBufferPooling\": %s,\n"
        "  \"bufferPoolSize\": %d,\n"
        "  \"enableGPUSpecificTweaks\": %s,\n"
        "  \"enableDebugOutput\": %s,\n"
        "  \"enableProfiling\": %s\n"
        "}\n",
        config->quality,
        config->backend,
        config->shaderCache,
        config->shaderCachePath ? config->shaderCachePath : "",
        config->shaderCacheMaxSize,
        config->enableDynamicResolution ? "true" : "false",
        config->minResolutionScale,
        config->maxResolutionScale,
        config->targetFPS,
        config->enableDrawBatching ? "true" : "false",
        config->enableInstancing ? "true" : "false",
        config->maxBatchSize,
        config->enableTextureCompression ? "true" : "false",
        config->texturePoolSize,
        config->maxTextureSize,
        config->enableBufferPooling ? "true" : "false",
        config->bufferPoolSize,
        config->enableGPUSpecificTweaks ? "true" : "false",
        config->enableDebugOutput ? "true" : "false",
        config->enableProfiling ? "true" : "false"
    );
    
    if (len >= (int)sizeof(buffer)) {
        velocityLogError("Config buffer overflow");
        return false;
    }
    
    bool result = writeFile(path, buffer, len);
    
    if (result) {
        velocityLogInfo("Configuration saved successfully");
    }
    
    return result;
}

// ============================================================================
// Preset Configurations
// ============================================================================

VelocityConfig velocityConfigGetPreset(VelocityQualityPreset preset) {
    VelocityConfig config = velocityGetDefaultConfig();
    config.quality = preset;
    
    switch (preset) {
        case VELOCITY_QUALITY_ULTRA_LOW:
            config.minResolutionScale = 0.25f;
            config.maxResolutionScale = 0.5f;
            config.targetFPS = 30;
            config.enableDrawBatching = true;
            config.enableInstancing = false;
            config.maxBatchSize = 32;
            config.texturePoolSize = 32;
            config.maxTextureSize = 1024;
            config.bufferPoolSize = 8;
            break;
            
        case VELOCITY_QUALITY_LOW:
            config.minResolutionScale = 0.4f;
            config.maxResolutionScale = 0.7f;
            config.targetFPS = 30;
            config.enableDrawBatching = true;
            config.enableInstancing = true;
            config.maxBatchSize = 64;
            config.texturePoolSize = 64;
            config.maxTextureSize = 2048;
            config.bufferPoolSize = 16;
            break;
            
        case VELOCITY_QUALITY_MEDIUM:
            config.minResolutionScale = 0.5f;
            config.maxResolutionScale = 1.0f;
            config.targetFPS = 45;
            config.enableDrawBatching = true;
            config.enableInstancing = true;
            config.maxBatchSize = 128;
            config.texturePoolSize = 128;
            config.maxTextureSize = 4096;
            config.bufferPoolSize = 32;
            break;
            
        case VELOCITY_QUALITY_HIGH:
            config.minResolutionScale = 0.7f;
            config.maxResolutionScale = 1.0f;
            config.targetFPS = 60;
            config.enableDrawBatching = true;
            config.enableInstancing = true;
            config.maxBatchSize = 192;
            config.texturePoolSize = 192;
            config.maxTextureSize = 4096;
            config.bufferPoolSize = 48;
            break;
            
        case VELOCITY_QUALITY_ULTRA:
            config.minResolutionScale = 0.85f;
            config.maxResolutionScale = 1.0f;
            config.enableDynamicResolution = false;
            config.targetFPS = 60;
            config.enableDrawBatching = true;
            config.enableInstancing = true;
            config.maxBatchSize = 256;
            config.texturePoolSize = 256;
            config.maxTextureSize = 8192;
            config.bufferPoolSize = 64;
            break;
            
        default:
            break;
    }
    
    return config;
}

void velocityConfigApplyGPURecommended(VelocityConfig* config) {
    if (!config) return;
    gpuGetRecommendedSettings(config);
}
