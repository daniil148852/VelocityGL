/**
 * VelocityGL Configuration System
 */

#include "config.h"
#include "log.h"
#include "memory.h"
#include "../gpu/gpu_detect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

// ============================================================================
// Simple JSON Parser
// ============================================================================

typedef enum JsonTokenType {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING,
    JSON_ARRAY_START, JSON_ARRAY_END, JSON_OBJECT_START, JSON_OBJECT_END,
    JSON_COLON, JSON_COMMA, JSON_EOF, JSON_ERROR
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

static void skipWhitespace(JsonParser* p) {
    while (p->pos < p->length && isspace(p->data[p->pos])) {
        p->pos++;
    }
}

static JsonToken parseString(JsonParser* p) {
    JsonToken token = {JSON_STRING};
    p->pos++; // Skip quote
    size_t start = p->pos;
    while (p->pos < p->length && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\') p->pos++;
        p->pos++;
    }
    size_t len = p->pos - start;
    token.stringValue = (char*)velocityMalloc(len + 1);
    
    // Simplified copy (no escape handling for brevity)
    strncpy(token.stringValue, p->data + start, len);
    token.stringValue[len] = '\0';
    
    p->pos++; // Skip closing quote
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
        p->pos += 4; return (JsonToken){JSON_BOOL, .boolValue = true};
    }
    if (strncmp(p->data + p->pos, "false", 5) == 0) {
        p->pos += 5; return (JsonToken){JSON_BOOL, .boolValue = false};
    }
    return (JsonToken){JSON_NULL};
}

static JsonToken nextToken(JsonParser* p) {
    skipWhitespace(p);
    if (p->pos >= p->length) return (JsonToken){JSON_EOF};
    char c = p->data[p->pos];
    switch (c) {
        case '{': p->pos++; return (JsonToken){JSON_OBJECT_START};
        case '}': p->pos++; return (JsonToken){JSON_OBJECT_END};
        case '[': p->pos++; return (JsonToken){JSON_ARRAY_START};
        case ']': p->pos++; return (JsonToken){JSON_ARRAY_END};
        case ':': p->pos++; return (JsonToken){JSON_COLON};
        case ',': p->pos++; return (JsonToken){JSON_COMMA};
        case '"': return parseString(p);
        case '-': case '0' ... '9': return parseNumber(p);
        case 't': case 'f': case 'n': return parseKeyword(p);
        default: return (JsonToken){JSON_ERROR};
    }
}

// ============================================================================
// File I/O
// ============================================================================

static char* readFile(const char* path, size_t* outSize) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* data = (char*)velocityMalloc(size + 1);
    fread(data, 1, size, file);
    fclose(file);
    data[size] = '\0';
    if (outSize) *outSize = size;
    return data;
}

// ============================================================================
// Config Implementation
// ============================================================================

bool velocityConfigLoad(const char* path, VelocityConfig* config) {
    size_t size;
    char* data = readFile(path, &size);
    if (!data) return false;
    
    JsonParser parser = { .data = data, .pos = 0, .length = size };
    JsonToken token = nextToken(&parser);
    
    // Very simplified parser loop
    while ((token = nextToken(&parser)).type != JSON_EOF) {
        if (token.type == JSON_STRING) {
            char* key = token.stringValue;
            nextToken(&parser); // skip colon
            token = nextToken(&parser); // value
            
            if (strcmp(key, "targetFPS") == 0) config->targetFPS = (int)token.numberValue;
            else if (strcmp(key, "quality") == 0) config->quality = (int)token.numberValue;
            
            velocityFree(key);
            if (token.type == JSON_STRING) velocityFree(token.stringValue);
        }
    }
    
    velocityFree(data);
    return true;
}

bool velocityConfigSave(const char* path, const VelocityConfig* config) {
    return false; // Stub
}

VelocityConfig velocityConfigGetPreset(VelocityQualityPreset preset) {
    VelocityConfig config = velocityGetDefaultConfig();
    config.quality = preset;
    return config;
}

void velocityConfigApplyGPURecommended(VelocityConfig* config) {
    if (!config) return;
    gpuGetRecommendedSettings(config);
}
