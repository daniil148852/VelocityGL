/**
 * Draw Batcher - Combines multiple draw calls into batched operations
 * Reduces driver overhead and improves performance
 */

#ifndef DRAW_BATCHER_H
#define DRAW_BATCHER_H

#include <GLES3/gl32.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define MAX_BATCH_COMMANDS      1024
#define MAX_BATCH_VERTICES      65536
#define MAX_BATCH_INDICES       131072
#define MAX_BATCH_INSTANCES     4096
#define VERTEX_BUFFER_SIZE      (16 * 1024 * 1024)  // 16 MB
#define INDEX_BUFFER_SIZE       (4 * 1024 * 1024)   // 4 MB

// ============================================================================
// Types
// ============================================================================

/**
 * Draw command type
 */
typedef enum DrawCommandType {
    DRAW_CMD_ARRAYS,
    DRAW_CMD_ELEMENTS,
    DRAW_CMD_ARRAYS_INSTANCED,
    DRAW_CMD_ELEMENTS_INSTANCED,
    DRAW_CMD_MULTI_DRAW_ARRAYS,
    DRAW_CMD_MULTI_DRAW_ELEMENTS,
    DRAW_CMD_INDIRECT
} DrawCommandType;

/**
 * Primitive mode
 */
typedef enum PrimitiveMode {
    PRIM_TRIANGLES = GL_TRIANGLES,
    PRIM_TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
    PRIM_TRIANGLE_FAN = GL_TRIANGLE_FAN,
    PRIM_LINES = GL_LINES,
    PRIM_LINE_STRIP = GL_LINE_STRIP,
    PRIM_POINTS = GL_POINTS
} PrimitiveMode;

/**
 * Vertex format element
 */
typedef struct VertexElement {
    GLuint index;           // Attribute index
    GLint size;             // Component count (1-4)
    GLenum type;            // GL_FLOAT, etc
    GLboolean normalized;
    GLsizei stride;
    size_t offset;
} VertexElement;

/**
 * Vertex format descriptor
 */
typedef struct VertexFormat {
    VertexElement elements[16];
    int elementCount;
    GLsizei stride;         // Total stride
    uint64_t hash;          // For quick comparison
} VertexFormat;

/**
 * Batch key - defines what can be batched together
 */
typedef struct BatchKey {
    GLuint program;
    GLuint vao;
    GLuint texture0;        // Main texture
    GLuint texture1;        // Additional textures
    PrimitiveMode mode;
    uint64_t stateHash;     // Blend, depth, etc state
} BatchKey;

/**
 * Draw command for indirect drawing
 */
typedef struct DrawArraysIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
} DrawArraysIndirectCommand;

typedef struct DrawElementsIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
} DrawElementsIndirectCommand;

/**
 * Single draw command in batch
 */
typedef struct DrawCommand {
    DrawCommandType type;
    PrimitiveMode mode;
    
    // For glDrawArrays
    GLint first;
    GLsizei count;
    
    // For glDrawElements
    GLenum indexType;
    const void* indices;
    
    // For instanced
    GLsizei instanceCount;
    GLuint baseInstance;
    
    // For batching
    BatchKey key;
    bool canBatch;
    
    // Vertex data (for dynamic batching)
    const void* vertexData;
    size_t vertexDataSize;
    const void* indexData;
    size_t indexDataSize;
} DrawCommand;

/**
 * Batched draw call
 */
typedef struct BatchedDraw {
    BatchKey key;
    DrawArraysIndirectCommand* arrayCommands;
    DrawElementsIndirectCommand* elementCommands;
    int commandCount;
    bool isElements;
} BatchedDraw;

/**
 * Draw batcher context
 */
typedef struct DrawBatcherContext {
    // Command queue
    DrawCommand* commands;
    int commandCount;
    int maxCommands;
    
    // Batched results
    BatchedDraw* batches;
    int batchCount;
    int maxBatches;
    
    // Vertex/index buffers for dynamic batching
    GLuint vertexBuffer;
    GLuint indexBuffer;
    size_t vertexOffset;
    size_t indexOffset;
    void* vertexMapped;
    void* indexMapped;
    
    // Indirect command buffer
    GLuint indirectBuffer;
    size_t indirectOffset;
    
    // Statistics
    uint32_t drawCallsSubmitted;
    uint32_t drawCallsExecuted;
    uint32_t drawCallsSaved;
    uint32_t batchesCreated;
    
    // Configuration
    bool enableBatching;
    bool enableInstancing;
    int minBatchSize;       // Minimum commands to batch
    
    bool initialized;
} DrawBatcherContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize draw batcher
 */
bool drawBatcherInit(int maxCommands);

/**
 * Shutdown draw batcher
 */
void drawBatcherShutdown(void);

/**
 * Begin new frame
 */
void drawBatcherBeginFrame(void);

/**
 * End frame and execute batched draws
 */
void drawBatcherEndFrame(void);

/**
 * Submit draw command
 */
void drawBatcherSubmit(const DrawCommand* cmd);

/**
 * Submit glDrawArrays
 */
void drawBatcherDrawArrays(GLenum mode, GLint first, GLsizei count);

/**
 * Submit glDrawElements
 */
void drawBatcherDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);

/**
 * Submit glDrawArraysInstanced
 */
void drawBatcherDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount);

/**
 * Submit glDrawElementsInstanced
 */
void drawBatcherDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, 
                                        const void* indices, GLsizei instanceCount);

/**
 * Flush pending commands immediately
 */
void drawBatcherFlush(void);

/**
 * Set current batch key (program, textures, state)
 */
void drawBatcherSetKey(const BatchKey* key);

/**
 * Enable/disable batching
 */
void drawBatcherSetEnabled(bool enabled);

/**
 * Enable/disable instancing
 */
void drawBatcherSetInstancing(bool enabled);

// ============================================================================
// Vertex Format Management
// ============================================================================

/**
 * Create vertex format descriptor
 */
VertexFormat* vertexFormatCreate(void);

/**
 * Add element to vertex format
 */
void vertexFormatAddElement(VertexFormat* format, GLuint index, GLint size, 
                            GLenum type, GLboolean normalized, size_t offset);

/**
 * Finalize vertex format
 */
void vertexFormatFinalize(VertexFormat* format);

/**
 * Compare vertex formats
 */
bool vertexFormatEquals(const VertexFormat* a, const VertexFormat* b);

/**
 * Apply vertex format to VAO
 */
void vertexFormatApply(const VertexFormat* format, GLuint vao, GLuint vbo);

// ============================================================================
// Statistics
// ============================================================================

/**
 * Get batcher statistics
 */
void drawBatcherGetStats(uint32_t* submitted, uint32_t* executed, 
                         uint32_t* saved, uint32_t* batches);

/**
 * Reset statistics
 */
void drawBatcherResetStats(void);

#ifdef __cplusplus
}
#endif

#endif // DRAW_BATCHER_H
