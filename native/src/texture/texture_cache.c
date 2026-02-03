/**
 * Texture Cache - Stub implementation
 */

#include "texture_manager.h"
#include "../utils/log.h"

// ============================================================================
// Cache Implementation
// ============================================================================

Texture* textureCacheGet(uint64_t hash) {
    // Stub - would search cache
    return NULL;
}

void textureCacheAdd(Texture* texture, uint64_t hash) {
    if (!texture) return;
    texture->hash = hash;
    // Stub - would add to cache
}

void textureCacheClear(void) {
    velocityLogInfo("Clearing texture cache");
    // Stub - would clear cache
}
