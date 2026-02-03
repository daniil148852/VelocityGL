/**
 * Simple GLSL Parser for shader analysis
 */

#include "shader_cache.h"
#include "../utils/log.h"
#include "../utils/memory.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
// Token Types
// ============================================================================

typedef enum {
    TOKEN_NONE,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_OPERATOR,
    TOKEN_PREPROCESSOR,
    TOKEN_COMMENT,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
} Token;

// ============================================================================
// Lexer State
// ============================================================================

typedef struct {
    const char* source;
    size_t pos;
    size_t length;
    int line;
    int column;
} Lexer;

// ============================================================================
// Lexer Functions
// ============================================================================

static void lexerInit(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->length = strlen(source);
    lexer->line = 1;
    lexer->column = 1;
}

static char lexerPeek(Lexer* lexer) {
    if (lexer->pos >= lexer->length) return '\0';
    return lexer->source[lexer->pos];
}

static char lexerAdvance(Lexer* lexer) {
    if (lexer->pos >= lexer->length) return '\0';
    
    char c = lexer->source[lexer->pos++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void lexerSkipWhitespace(Lexer* lexer) {
    while (lexer->pos < lexer->length) {
        char c = lexerPeek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lexerAdvance(lexer);
        } else if (c == '/' && lexer->pos + 1 < lexer->length) {
            char next = lexer->source[lexer->pos + 1];
            if (next == '/') {
                // Line comment
                while (lexer->pos < lexer->length && lexerPeek(lexer) != '\n') {
                    lexerAdvance(lexer);
                }
            } else if (next == '*') {
                // Block comment
                lexerAdvance(lexer);
                lexerAdvance(lexer);
                while (lexer->pos + 1 < lexer->length) {
                    if (lexerPeek(lexer) == '*' && lexer->source[lexer->pos + 1] == '/') {
                        lexerAdvance(lexer);
                        lexerAdvance(lexer);
                        break;
                    }
                    lexerAdvance(lexer);
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static Token lexerNextToken(Lexer* lexer) {
    Token token = {TOKEN_NONE, NULL, 0, 0};
    
    lexerSkipWhitespace(lexer);
    
    if (lexer->pos >= lexer->length) {
        token.type = TOKEN_EOF;
        return token;
    }
    
    token.line = lexer->line;
    token.column = lexer->column;
    
    char c = lexerPeek(lexer);
    
    // Preprocessor
    if (c == '#') {
        size_t start = lexer->pos;
        while (lexer->pos < lexer->length && lexerPeek(lexer) != '\n') {
            lexerAdvance(lexer);
        }
        size_t len = lexer->pos - start;
        token.type = TOKEN_PREPROCESSOR;
        token.value = (char*)velocityMalloc(len + 1);
        memcpy(token.value, lexer->source + start, len);
        token.value[len] = '\0';
        return token;
    }
    
    // Identifier
    if (isalpha(c) || c == '_') {
        size_t start = lexer->pos;
        while (lexer->pos < lexer->length) {
            c = lexerPeek(lexer);
            if (!isalnum(c) && c != '_') break;
            lexerAdvance(lexer);
        }
        size_t len = lexer->pos - start;
        token.type = TOKEN_IDENTIFIER;
        token.value = (char*)velocityMalloc(len + 1);
        memcpy(token.value, lexer->source + start, len);
        token.value[len] = '\0';
        return token;
    }
    
    // Number
    if (isdigit(c) || (c == '.' && lexer->pos + 1 < lexer->length && isdigit(lexer->source[lexer->pos + 1]))) {
        size_t start = lexer->pos;
        while (lexer->pos < lexer->length) {
            c = lexerPeek(lexer);
            if (!isdigit(c) && c != '.' && c != 'e' && c != 'E' && c != '-' && c != '+' && c != 'f' && c != 'F') break;
            lexerAdvance(lexer);
        }
        size_t len = lexer->pos - start;
        token.type = TOKEN_NUMBER;
        token.value = (char*)velocityMalloc(len + 1);
        memcpy(token.value, lexer->source + start, len);
        token.value[len] = '\0';
        return token;
    }
    
    // Operator
    token.type = TOKEN_OPERATOR;
    token.value = (char*)velocityMalloc(3);
    token.value[0] = lexerAdvance(lexer);
    token.value[1] = '\0';
    
    return token;
}

// ============================================================================
// Shader Analysis
// ============================================================================

typedef struct {
    char** uniforms;
    int uniformCount;
    char** attributes;
    int attributeCount;
    char** varyings;
    int varyingCount;
    int version;
    bool usesGeometry;
    bool usesTessellation;
    bool usesCompute;
} ShaderInfo;

ShaderInfo* shaderParse(const char* source) {
    if (!source) return NULL;
    
    ShaderInfo* info = (ShaderInfo*)velocityCalloc(1, sizeof(ShaderInfo));
    
    Lexer lexer;
    lexerInit(&lexer, source);
    
    Token token;
    while ((token = lexerNextToken(&lexer)).type != TOKEN_EOF) {
        if (token.type == TOKEN_PREPROCESSOR) {
            // Check for #version
            if (strncmp(token.value, "#version", 8) == 0) {
                info->version = atoi(token.value + 8);
            }
        } else if (token.type == TOKEN_IDENTIFIER) {
            // Check for uniform declarations
            if (strcmp(token.value, "uniform") == 0) {
                velocityFree(token.value);
                
                // Skip type
                token = lexerNextToken(&lexer);
                if (token.type == TOKEN_IDENTIFIER) {
                    velocityFree(token.value);
                }
                
                // Get name
                token = lexerNextToken(&lexer);
                if (token.type == TOKEN_IDENTIFIER) {
                    info->uniforms = (char**)velocityRealloc(info->uniforms, 
                        (info->uniformCount + 1) * sizeof(char*));
                    info->uniforms[info->uniformCount++] = token.value;
                    token.value = NULL;
                }
            }
            // Check for in/attribute declarations
            else if (strcmp(token.value, "in") == 0 || strcmp(token.value, "attribute") == 0) {
                velocityFree(token.value);
                
                // Skip type
                token = lexerNextToken(&lexer);
                if (token.type == TOKEN_IDENTIFIER) {
                    velocityFree(token.value);
                }
                
                // Get name
                token = lexerNextToken(&lexer);
                if (token.type == TOKEN_IDENTIFIER) {
                    info->attributes = (char**)velocityRealloc(info->attributes,
                        (info->attributeCount + 1) * sizeof(char*));
                    info->attributes[info->attributeCount++] = token.value;
                    token.value = NULL;
                }
            }
        }
        
        if (token.value) {
            velocityFree(token.value);
        }
    }
    
    return info;
}

void shaderInfoFree(ShaderInfo* info) {
    if (!info) return;
    
    for (int i = 0; i < info->uniformCount; i++) {
        velocityFree(info->uniforms[i]);
    }
    velocityFree(info->uniforms);
    
    for (int i = 0; i < info->attributeCount; i++) {
        velocityFree(info->attributes[i]);
    }
    velocityFree(info->attributes);
    
    for (int i = 0; i < info->varyingCount; i++) {
        velocityFree(info->varyings[i]);
    }
    velocityFree(info->varyings);
    
    velocityFree(info);
}
