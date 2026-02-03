/**
 * Texture Compression - Stub implementation
 */

#include "texture_manager.h"
#include "../utils/log.h"

// ============================================================================
// Compression Detection
// ============================================================================

bool textureFormatIsCompressed(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_ETC2_RGB:
        case TEX_FORMAT_ETC2_RGBA:
        case TEX_FORMAT_ASTC_4x4:
        case TEX_FORMAT_ASTC_6x6:
        case TEX_FORMAT_ASTC_8x8:
            return true;
        default:
            return false;
    }
}

int textureCompressedBlockSize(TextureFormat format) {
    switch (format) {
        case TEX_FORMAT_ETC2_RGB:
        case TEX_FORMAT_ETC2_RGBA:
            return 4;
        case TEX_FORMAT_ASTC_4x4:
            return 4;
        case TEX_FORMAT_ASTC_6x6:
            return 6;
        case TEX_FORMAT_ASTC_8x8:
            return 8;
        default:
            return 1;
    }
}
