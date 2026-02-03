/**
 * VelocityGL Logging System - Implementation
 */

#include "log.h"

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

// ============================================================================
// Internal State
// ============================================================================

typedef struct LogContext {
    VelocityLogLevel minLevel;
    FILE* logFile;
    char* logPath;
    pthread_mutex_t mutex;
    bool initialized;
    
    // Ring buffer for async logging
    char buffer[VELOCITY_LOG_BUFFER_SIZE][VELOCITY_LOG_MAX_LENGTH];
    int bufferHead;
    int bufferTail;
    
} LogContext;

static LogContext g_log = {
    .minLevel = VELOCITY_LOG_INFO,
    .logFile = NULL,
    .logPath = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false,
    .bufferHead = 0,
    .bufferTail = 0
};

// ============================================================================
// Android Log Priority Mapping
// ============================================================================

static int getAndroidPriority(VelocityLogLevel level) {
    switch (level) {
        case VELOCITY_LOG_VERBOSE: return ANDROID_LOG_VERBOSE;
        case VELOCITY_LOG_DEBUG:   return ANDROID_LOG_DEBUG;
        case VELOCITY_LOG_INFO:    return ANDROID_LOG_INFO;
        case VELOCITY_LOG_WARN:    return ANDROID_LOG_WARN;
        case VELOCITY_LOG_ERROR:   return ANDROID_LOG_ERROR;
        case VELOCITY_LOG_FATAL:   return ANDROID_LOG_FATAL;
        default:                   return ANDROID_LOG_DEFAULT;
    }
}

const char* velocityLogLevelName(VelocityLogLevel level) {
    switch (level) {
        case VELOCITY_LOG_VERBOSE: return "VERBOSE";
        case VELOCITY_LOG_DEBUG:   return "DEBUG";
        case VELOCITY_LOG_INFO:    return "INFO";
        case VELOCITY_LOG_WARN:    return "WARN";
        case VELOCITY_LOG_ERROR:   return "ERROR";
        case VELOCITY_LOG_FATAL:   return "FATAL";
        default:                   return "UNKNOWN";
    }
}

// ============================================================================
// Initialization
// ============================================================================

void velocityLogInit(const char* logPath, VelocityLogLevel minLevel) {
    pthread_mutex_lock(&g_log.mutex);
    
    g_log.minLevel = minLevel;
    
    if (logPath && logPath[0] != '\0') {
        g_log.logPath = strdup(logPath);
        g_log.logFile = fopen(logPath, "a");
        if (!g_log.logFile) {
            __android_log_print(ANDROID_LOG_ERROR, VELOCITY_LOG_TAG,
                               "Failed to open log file: %s", logPath);
        }
    }
    
    g_log.initialized = true;
    
    pthread_mutex_unlock(&g_log.mutex);
    
    velocityLogInfo("=== VelocityGL Log Started ===");
}

void velocityLogShutdown(void) {
    velocityLogInfo("=== VelocityGL Log Ended ===");
    
    pthread_mutex_lock(&g_log.mutex);
    
    if (g_log.logFile) {
        fflush(g_log.logFile);
        fclose(g_log.logFile);
        g_log.logFile = NULL;
    }
    
    if (g_log.logPath) {
        free(g_log.logPath);
        g_log.logPath = NULL;
    }
    
    g_log.initialized = false;
    
    pthread_mutex_unlock(&g_log.mutex);
}

void velocityLogSetLevel(VelocityLogLevel level) {
    g_log.minLevel = level;
}

VelocityLogLevel velocityLogGetLevel(void) {
    return g_log.minLevel;
}

void velocityLogSetFileOutput(const char* path) {
    pthread_mutex_lock(&g_log.mutex);
    
    // Close existing file
    if (g_log.logFile) {
        fclose(g_log.logFile);
        g_log.logFile = NULL;
    }
    
    if (g_log.logPath) {
        free(g_log.logPath);
        g_log.logPath = NULL;
    }
    
    // Open new file
    if (path && path[0] != '\0') {
        g_log.logPath = strdup(path);
        g_log.logFile = fopen(path, "a");
    }
    
    pthread_mutex_unlock(&g_log.mutex);
}

// ============================================================================
// Core Logging
// ============================================================================

void velocityLogV(VelocityLogLevel level, const char* fmt, va_list args) {
    if (level < g_log.minLevel) {
        return;
    }
    
    char message[VELOCITY_LOG_MAX_LENGTH];
    vsnprintf(message, sizeof(message), fmt, args);
    
    // Always log to Android logcat
    __android_log_print(getAndroidPriority(level), VELOCITY_LOG_TAG, "%s", message);
    
    // Log to file if enabled
    if (g_log.logFile) {
        pthread_mutex_lock(&g_log.mutex);
        
        // Get timestamp
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm;
        localtime_r(&ts.tv_sec, &tm);
        
        // Get thread ID
        pid_t tid = syscall(SYS_gettid);
        
        // Write to file
        fprintf(g_log.logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%d] %s: %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                ts.tv_nsec / 1000000,
                tid,
                velocityLogLevelName(level),
                message);
        
        // Flush on errors
        if (level >= VELOCITY_LOG_ERROR) {
            fflush(g_log.logFile);
        }
        
        pthread_mutex_unlock(&g_log.mutex);
    }
}

void velocityLog(VelocityLogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    velocityLogV(level, fmt, args);
    va_end(args);
}

// ============================================================================
// Utility Functions
// ============================================================================

void velocityLogHex(VelocityLogLevel level, const void* data, size_t size, const char* label) {
    if (level < g_log.minLevel || !data || size == 0) {
        return;
    }
    
    velocityLog(level, "%s (%zu bytes):", label ? label : "Data", size);
    
    const unsigned char* bytes = (const unsigned char*)data;
    char line[80];
    char* ptr;
    
    for (size_t i = 0; i < size; i += 16) {
        ptr = line;
        ptr += sprintf(ptr, "%04zx: ", i);
        
        // Hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                ptr += sprintf(ptr, "%02x ", bytes[i + j]);
            } else {
                ptr += sprintf(ptr, "   ");
            }
        }
        
        ptr += sprintf(ptr, " ");
        
        // ASCII
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = bytes[i + j];
            *ptr++ = (c >= 32 && c < 127) ? c : '.';
        }
        *ptr = '\0';
        
        velocityLog(level, "%s", line);
    }
}

void velocityLogGLError(unsigned int error, const char* context) {
    const char* errorStr;
    switch (error) {
        case 0x0500: errorStr = "GL_INVALID_ENUM"; break;
        case 0x0501: errorStr = "GL_INVALID_VALUE"; break;
        case 0x0502: errorStr = "GL_INVALID_OPERATION"; break;
        case 0x0503: errorStr = "GL_STACK_OVERFLOW"; break;
        case 0x0504: errorStr = "GL_STACK_UNDERFLOW"; break;
        case 0x0505: errorStr = "GL_OUT_OF_MEMORY"; break;
        case 0x0506: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        default:     errorStr = "UNKNOWN_ERROR"; break;
    }
    
    velocityLogError("GL Error %s (0x%04x) at %s", errorStr, error, context ? context : "unknown");
}

void velocityLogFlush(void) {
    pthread_mutex_lock(&g_log.mutex);
    
    if (g_log.logFile) {
        fflush(g_log.logFile);
    }
    
    pthread_mutex_unlock(&g_log.mutex);
}
