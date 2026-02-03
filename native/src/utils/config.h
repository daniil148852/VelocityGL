/**
 * VelocityGL Configuration Header
 */

#ifndef VELOCITY_CONFIG_H
#define VELOCITY_CONFIG_H

#include "../velocity_gl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load configuration from JSON file
 */
bool velocityConfigLoad(const char* path, VelocityConfig* config);

/**
 * Save configuration to JSON file
 */
bool velocityConfigSave(const char* path, const VelocityConfig* config);

/**
 * Get preset configuration
 */
VelocityConfig velocityConfigGetPreset(VelocityQualityPreset preset);

/**
 * Apply GPU-recommended settings
 */
void velocityConfigApplyGPURecommended(VelocityConfig* config);

#ifdef __cplusplus
}
#endif

#endif // VELOCITY_CONFIG_H
