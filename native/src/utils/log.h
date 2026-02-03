/**
 * VelocityGL Logging System
 * Thread-safe logging with Android logcat integration
 */

#ifndef VELOCITY_LOG_H
#define VELOCITY_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Log Levels
// ============================================================================

typedef enum VelocityLogLevel {
    VELOCITY_LOG_VERBOSE = 0,
    VELOCITY_LOG_DEBUG,
    VELOCITY_LOG_INFO,
    VELOCITY_LOG_WARN,
    VELOCITY_LOG_ERROR,
    VELOCITY_LOG_FATAL,
    VELOCITY_LOG_SILENT
} VelocityLogLevel;

// ============================================================================
// Configuration
// ============================================================================

#define VELOCITY_LOG_TAG "VelocityGL"
#define VELOCITY_LOG_MAX_LENGTH 1024
#define VELOCITY_LOG_BUFFER_SIZE 64

// ============================================================================
// Log Macros
// ============================================================================

#ifdef VELOCITY_DEBUG_LOG
    #define velocityLogVerbose(fmt, ...) velocityLog(VELOCITY_LOG_VERBOSE, fmt, ##__VA_ARGS__)
    #define velocityLogDebug(fmt, ...)   velocityLog(VELOCITY_LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define velocityLogVerbose(fmt, ...) ((void)0)
    #define velocityLogDebug(fmt, ...)   ((void)0)
#endif

#define velocityLogInfo(fmt, ...)  velocityLog(VELOCITY_LOG_INFO, fmt, ##__VA_ARGS__)
#define velocityLogWarn(fmt, ...)  velocityLog(VELOCITY_LOG_WARN, fmt, ##__VA_ARGS__)
#define velocityLogError(fmt, ...) velocityLog(VELOCITY_LOG_ERROR, fmt, ##__VA_ARGS__)
#define velocityLogFatal(fmt, ...) velocityLog(VELOCITY_LOG_FATAL, fmt, ##__VA_ARGS__)

// ============================================================================
// API Functions
// ============================================================================

/**
 * Initialize logging system
 * @param logPath Optional file path for log output (NULL for logcat only)
 * @param minLevel Minimum log level to output
 */
void velocityLogInit(const char* logPath, VelocityLogLevel minLevel);

/**
 * Shutdown logging system
 */
void velocityLogShutdown(void);

/**
 * Set minimum log level
 */
void velocityLogSetLevel(VelocityLogLevel level);

/**
 * Get current log level
 */
VelocityLogLevel velocityLogGetLevel(void);

/**
 * Enable/disable file logging
 */
void velocityLogSetFileOutput(const char* path);

/**
 * Main log function
 */
void velocityLog(VelocityLogLevel level, const char* fmt, ...);

/**
 * Log with va_list
 */
void velocityLogV(VelocityLogLevel level, const char* fmt, va_list args);

/**
 * Log binary data as hex dump
 */
void velocityLogHex(VelocityLogLevel level, const void* data, size_t size, const char* label);

/**
 * Log GL error with description
 */
void velocityLogGLError(unsigned int error, const char* context);

/**
 * Flush log buffer to file
 */
void velocityLogFlush(void);

/**
 * Get log level name
 */
const char* velocityLogLevelName(VelocityLogLevel level);

#ifdef __cplusplus
}
#endif

#endif // VELOCITY_LOG_H
