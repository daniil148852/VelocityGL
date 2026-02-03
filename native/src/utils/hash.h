/**
 * Hash Functions Header
 */

#ifndef VELOCITY_HASH_H
#define VELOCITY_HASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * FNV-1a hash for arbitrary data
 */
uint64_t hashFNV1a(const void* data, size_t size);

/**
 * Hash a null-terminated string
 */
uint64_t hashString(const char* str);

/**
 * Combine two hashes
 */
uint64_t hashCombine(uint64_t h1, uint64_t h2);

/**
 * MurmurHash3 for better distribution
 */
uint64_t hashMurmur3(const void* key, size_t len, uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif // VELOCITY_HASH_H
