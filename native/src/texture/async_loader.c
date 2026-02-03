/**
 * Async Texture Loader - Stub implementation
 */

#include "texture_manager.h"
#include "../utils/log.h"

// ============================================================================
// Async Loading Stubs
// ============================================================================

AsyncTextureRequest* textureLoadAsync(const void* data, size_t dataSize,
                                       const TextureParams* params,
                                       void (*callback)(Texture*, void*),
                                       void* userData) {
    // Stub - would queue async load
    velocityLogDebug("Async texture load requested");
    return NULL;
}

void textureLoadAsyncCancel(AsyncTextureRequest* request) {
    if (request) {
        request->cancelled = true;
    }
}

void textureProcessAsyncLoads(void) {
    // Stub - would process completed loads
}
