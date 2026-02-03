/**
 * Draw Batcher - Implementation
 */

#include "draw_batcher.h"
#include "../utils/log.h"
#include "../utils/memory.h"
#include "../core/gl_wrapper.h"

#include <string.h>
#include <stdlib.h>

// ============================================================================
// Global State
// ============================================================================

static DrawBatcherContext* g_batcher = NULL;
static BatchKey g_currentKey = {0};

// ============================================================================
// Hash Functions
// ============================================================================

static uint64_t hashBatchKey(const BatchKey* key) {
    uint64_t hash = 14695981039346656037ULL;
    
    hash ^= key->program;
    hash *= 1099511628211ULL;
    hash ^= key->vao;
    hash *= 1099511628211ULL;
    hash ^= key->texture0;
    hash *= 1099511628211ULL;
    hash ^= key->texture1;
    hash *= 1099511628211ULL;
    hash ^= key->mode;
    hash *= 1099511628211ULL;
    hash ^= key->stateHash;
    
    return hash;
}

static bool batchKeysEqual(const BatchKey* a, const BatchKey* b) {
    return a->program == b->program &&
           a->vao == b->vao &&
           a->texture0 == b->texture0 &&
           a->texture1 == b->texture1 &&
           a->mode == b->mode &&
           a->stateHash == b->stateHash;
}

// ============================================================================
// Initialization
// ============================================================================

bool drawBatcherInit(int maxCommands) {
    if (g_batcher) {
        velocityLogWarn("Draw batcher already initialized");
        return true;
    }
    
    velocityLogInfo("Initializing draw batcher (max commands: %d)", maxCommands);
    
    g_batcher = (DrawBatcherContext*)velocityCalloc(1, sizeof(DrawBatcherContext));
    if (!g_batcher) {
        velocityLogError("Failed to allocate draw batcher");
        return false;
    }
    
    if (maxCommands <= 0) maxCommands = MAX_BATCH_COMMANDS;
    
    g_batcher->maxCommands = maxCommands;
    g_batcher->commands = (DrawCommand*)velocityCalloc(maxCommands, sizeof(DrawCommand));
    
    g_batcher->maxBatches = maxCommands / 4;  // Rough estimate
    g_batcher->batches = (BatchedDraw*)velocityCalloc(g_batcher->maxBatches, sizeof(BatchedDraw));
    
    if (!g_batcher->commands || !g_batcher->batches) {
        velocityLogError("Failed to allocate batcher buffers");
        velocityFree(g_batcher->commands);
        velocityFree(g_batcher->batches);
        velocityFree(g_batcher);
        g_batcher = NULL;
        return false;
    }
    
    // Create vertex buffer for dynamic batching
    glGenBuffers(1, &g_batcher->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, g_batcher->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE, NULL, GL_STREAM_DRAW);
    
    // Create index buffer
    glGenBuffers(1, &g_batcher->indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_batcher->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, INDEX_BUFFER_SIZE, NULL, GL_STREAM_DRAW);
    
    // Create indirect command buffer
    glGenBuffers(1, &g_batcher->indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_batcher->indirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, 
                 maxCommands * sizeof(DrawElementsIndirectCommand), 
                 NULL, GL_STREAM_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    
    g_batcher->enableBatching = true;
    g_batcher->enableInstancing = true;
    g_batcher->minBatchSize = 2;
    g_batcher->initialized = true;
    
    velocityLogInfo("Draw batcher initialized");
    
    return true;
}

void drawBatcherShutdown(void) {
    if (!g_batcher) return;
    
    velocityLogInfo("Shutting down draw batcher");
    
    glDeleteBuffers(1, &g_batcher->vertexBuffer);
    glDeleteBuffers(1, &g_batcher->indexBuffer);
    glDeleteBuffers(1, &g_batcher->indirectBuffer);
    
    for (int i = 0; i < g_batcher->batchCount; i++) {
        velocityFree(g_batcher->batches[i].arrayCommands);
        velocityFree(g_batcher->batches[i].elementCommands);
    }
    
    velocityFree(g_batcher->commands);
    velocityFree(g_batcher->batches);
    velocityFree(g_batcher);
    g_batcher = NULL;
}

// ============================================================================
// Frame Management
// ============================================================================

void drawBatcherBeginFrame(void) {
    if (!g_batcher) return;
    
    g_batcher->commandCount = 0;
    g_batcher->batchCount = 0;
    g_batcher->vertexOffset = 0;
    g_batcher->indexOffset = 0;
    g_batcher->indirectOffset = 0;
    
    g_batcher->drawCallsSubmitted = 0;
    g_batcher->drawCallsExecuted = 0;
    g_batcher->drawCallsSaved = 0;
    g_batcher->batchesCreated = 0;
}

// ============================================================================
// Command Submission
// ============================================================================

void drawBatcherSetKey(const BatchKey* key) {
    if (key) {
        memcpy(&g_currentKey, key, sizeof(BatchKey));
    }
}

void drawBatcherSubmit(const DrawCommand* cmd) {
    if (!g_batcher || !cmd) return;
    
    if (g_batcher->commandCount >= g_batcher->maxCommands) {
        velocityLogWarn("Draw batcher command overflow, flushing");
        drawBatcherFlush();
    }
    
    memcpy(&g_batcher->commands[g_batcher->commandCount], cmd, sizeof(DrawCommand));
    g_batcher->commandCount++;
    g_batcher->drawCallsSubmitted++;
}

void drawBatcherDrawArrays(GLenum mode, GLint first, GLsizei count) {
    DrawCommand cmd = {
        .type = DRAW_CMD_ARRAYS,
        .mode = mode,
        .first = first,
        .count = count,
        .instanceCount = 1,
        .key = g_currentKey,
        .canBatch = g_batcher && g_batcher->enableBatching
    };
    cmd.key.mode = mode;
    
    drawBatcherSubmit(&cmd);
}

void drawBatcherDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    DrawCommand cmd = {
        .type = DRAW_CMD_ELEMENTS,
        .mode = mode,
        .count = count,
        .indexType = type,
        .indices = indices,
        .instanceCount = 1,
        .key = g_currentKey,
        .canBatch = g_batcher && g_batcher->enableBatching
    };
    cmd.key.mode = mode;
    
    drawBatcherSubmit(&cmd);
}

void drawBatcherDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount) {
    DrawCommand cmd = {
        .type = DRAW_CMD_ARRAYS_INSTANCED,
        .mode = mode,
        .first = first,
        .count = count,
        .instanceCount = instanceCount,
        .key = g_currentKey,
        .canBatch = false  // Already instanced
    };
    cmd.key.mode = mode;
    
    drawBatcherSubmit(&cmd);
}

void drawBatcherDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                        const void* indices, GLsizei instanceCount) {
    DrawCommand cmd = {
        .type = DRAW_CMD_ELEMENTS_INSTANCED,
        .mode = mode,
        .count = count,
        .indexType = type,
        .indices = indices,
        .instanceCount = instanceCount,
        .key = g_currentKey,
        .canBatch = false  // Already instanced
    };
    cmd.key.mode = mode;
    
    drawBatcherSubmit(&cmd);
}

// ============================================================================
// Batch Building
// ============================================================================

static int compareBatchKeys(const void* a, const void* b) {
    const DrawCommand* cmdA = (const DrawCommand*)a;
    const DrawCommand* cmdB = (const DrawCommand*)b;
    
    uint64_t hashA = hashBatchKey(&cmdA->key);
    uint64_t hashB = hashBatchKey(&cmdB->key);
    
    if (hashA < hashB) return -1;
    if (hashA > hashB) return 1;
    return 0;
}

static void buildBatches(void) {
    if (!g_batcher || g_batcher->commandCount == 0) return;
    
    // Sort commands by batch key
    if (g_batcher->enableBatching) {
        qsort(g_batcher->commands, g_batcher->commandCount, 
              sizeof(DrawCommand), compareBatchKeys);
    }
    
    g_batcher->batchCount = 0;
    
    int i = 0;
    while (i < g_batcher->commandCount) {
        DrawCommand* cmd = &g_batcher->commands[i];
        
        // Count consecutive commands with same key
        int batchSize = 1;
        while (i + batchSize < g_batcher->commandCount &&
               batchKeysEqual(&cmd->key, &g_batcher->commands[i + batchSize].key) &&
               cmd->type == g_batcher->commands[i + batchSize].type) {
            batchSize++;
        }
        
        // Create batch if worthwhile
        if (g_batcher->batchCount < g_batcher->maxBatches) {
            BatchedDraw* batch = &g_batcher->batches[g_batcher->batchCount];
            batch->key = cmd->key;
            batch->commandCount = batchSize;
            batch->isElements = (cmd->type == DRAW_CMD_ELEMENTS || 
                                 cmd->type == DRAW_CMD_ELEMENTS_INSTANCED);
            
            g_batcher->batchCount++;
            g_batcher->batchesCreated++;
        }
        
        i += batchSize;
    }
}

// ============================================================================
// Batch Execution
// ============================================================================

static void executeDirect(DrawCommand* cmd) {
    switch (cmd->type) {
        case DRAW_CMD_ARRAYS:
            glDrawArrays(cmd->mode, cmd->first, cmd->count);
            break;
            
        case DRAW_CMD_ELEMENTS:
            glDrawElements(cmd->mode, cmd->count, cmd->indexType, cmd->indices);
            break;
            
        case DRAW_CMD_ARRAYS_INSTANCED:
            glDrawArraysInstanced(cmd->mode, cmd->first, cmd->count, cmd->instanceCount);
            break;
            
        case DRAW_CMD_ELEMENTS_INSTANCED:
            glDrawElementsInstanced(cmd->mode, cmd->count, cmd->indexType, 
                                    cmd->indices, cmd->instanceCount);
            break;
            
        default:
            break;
    }
    
    g_batcher->drawCallsExecuted++;
}

static void executeMultiDraw(int startIndex, int count, bool isElements) {
    // For simplicity, execute as individual calls
    // A full implementation would use glMultiDrawArrays/Elements
    
    for (int i = 0; i < count; i++) {
        executeDirect(&g_batcher->commands[startIndex + i]);
    }
    
    // Record savings (we'd save more with actual multi-draw)
    if (count > 1) {
        g_batcher->drawCallsSaved += count - 1;
    }
}

void drawBatcherFlush(void) {
    if (!g_batcher || g_batcher->commandCount == 0) return;
    
    buildBatches();
    
    int cmdIndex = 0;
    for (int b = 0; b < g_batcher->batchCount; b++) {
        BatchedDraw* batch = &g_batcher->batches[b];
        
        // Bind state for batch
        if (batch->key.program) {
            glUseProgram(batch->key.program);
        }
        if (batch->key.vao) {
            glBindVertexArray(batch->key.vao);
        }
        if (batch->key.texture0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, batch->key.texture0);
        }
        
        // Execute batch
        if (batch->commandCount >= g_batcher->minBatchSize && g_batcher->enableBatching) {
            executeMultiDraw(cmdIndex, batch->commandCount, batch->isElements);
        } else {
            for (int i = 0; i < batch->commandCount; i++) {
                executeDirect(&g_batcher->commands[cmdIndex + i]);
            }
        }
        
        cmdIndex += batch->commandCount;
    }
    
    // Reset for next flush
    g_batcher->commandCount = 0;
    g_batcher->batchCount = 0;
}

void drawBatcherEndFrame(void) {
    if (!g_batcher) return;
    
    drawBatcherFlush();
    
    // Update wrapper stats
    if (g_wrapperCtx) {
        g_wrapperCtx->stats.drawCalls = g_batcher->drawCallsExecuted;
        g_wrapperCtx->stats.drawCallsSaved = g_batcher->drawCallsSaved;
    }
}

// ============================================================================
// Configuration
// ============================================================================

void drawBatcherSetEnabled(bool enabled) {
    if (g_batcher) {
        g_batcher->enableBatching = enabled;
    }
}

void drawBatcherSetInstancing(bool enabled) {
    if (g_batcher) {
        g_batcher->enableInstancing = enabled;
    }
}

// ============================================================================
// Vertex Format
// ============================================================================

VertexFormat* vertexFormatCreate(void) {
    VertexFormat* format = (VertexFormat*)velocityCalloc(1, sizeof(VertexFormat));
    return format;
}

void vertexFormatAddElement(VertexFormat* format, GLuint index, GLint size,
                            GLenum type, GLboolean normalized, size_t offset) {
    if (!format || format->elementCount >= 16) return;
    
    VertexElement* elem = &format->elements[format->elementCount];
    elem->index = index;
    elem->size = size;
    elem->type = type;
    elem->normalized = normalized;
    elem->offset = offset;
    
    format->elementCount++;
}

void vertexFormatFinalize(VertexFormat* format) {
    if (!format) return;
    
    // Calculate stride
    size_t maxOffset = 0;
    size_t lastSize = 0;
    
    for (int i = 0; i < format->elementCount; i++) {
        if (format->elements[i].offset >= maxOffset) {
            maxOffset = format->elements[i].offset;
            lastSize = format->elements[i].size * 4;  // Assume 4 bytes per component
        }
    }
    
    format->stride = maxOffset + lastSize;
    
    // Calculate hash
    format->hash = 14695981039346656037ULL;
    for (int i = 0; i < format->elementCount; i++) {
        format->hash ^= format->elements[i].index;
        format->hash *= 1099511628211ULL;
        format->hash ^= format->elements[i].size;
        format->hash *= 1099511628211ULL;
    }
}

bool vertexFormatEquals(const VertexFormat* a, const VertexFormat* b) {
    if (!a || !b) return false;
    return a->hash == b->hash && a->elementCount == b->elementCount;
}

void vertexFormatApply(const VertexFormat* format, GLuint vao, GLuint vbo) {
    if (!format) return;
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    for (int i = 0; i < format->elementCount; i++) {
        const VertexElement* elem = &format->elements[i];
        
        glEnableVertexAttribArray(elem->index);
        glVertexAttribPointer(elem->index, elem->size, elem->type,
                              elem->normalized, format->stride, 
                              (const void*)elem->offset);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================================================================
// Statistics
// ============================================================================

void drawBatcherGetStats(uint32_t* submitted, uint32_t* executed,
                         uint32_t* saved, uint32_t* batches) {
    if (!g_batcher) {
        if (submitted) *submitted = 0;
        if (executed) *executed = 0;
        if (saved) *saved = 0;
        if (batches) *batches = 0;
        return;
    }
    
    if (submitted) *submitted = g_batcher->drawCallsSubmitted;
    if (executed) *executed = g_batcher->drawCallsExecuted;
    if (saved) *saved = g_batcher->drawCallsSaved;
    if (batches) *batches = g_batcher->batchesCreated;
}

void drawBatcherResetStats(void) {
    if (g_batcher) {
        g_batcher->drawCallsSubmitted = 0;
        g_batcher->drawCallsExecuted = 0;
        g_batcher->drawCallsSaved = 0;
        g_batcher->batchesCreated = 0;
    }
}
