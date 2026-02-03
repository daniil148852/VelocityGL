package com.velocitygl

import android.view.Surface
import androidx.annotation.Keep

/**
 * VelocityGL - Main JNI interface
 * Provides Kotlin/Java bindings to native library
 */
@Keep
object VelocityGL {

    // ========================================================================
    // Native method declarations
    // ========================================================================

    /**
     * Initialize VelocityGL with config path
     */
    @JvmStatic
    external fun nativeInit(configPath: String?): Boolean

    /**
     * Shutdown VelocityGL
     */
    @JvmStatic
    external fun nativeShutdown()

    /**
     * Create rendering context
     */
    @JvmStatic
    external fun nativeCreateContext(surface: Surface, eglDisplay: Long): Boolean

    /**
     * Destroy rendering context
     */
    @JvmStatic
    external fun nativeDestroyContext()

    /**
     * Swap buffers
     */
    @JvmStatic
    external fun nativeSwapBuffers()

    /**
     * Get GL function pointer by name
     */
    @JvmStatic
    external fun nativeGetProcAddress(name: String): Long

    /**
     * Get current FPS
     */
    @JvmStatic
    external fun nativeGetFPS(): Float

    /**
     * Set resolution scale
     */
    @JvmStatic
    external fun nativeSetResolutionScale(scale: Float)

    /**
     * Get resolution scale
     */
    @JvmStatic
    external fun nativeGetResolutionScale(): Float

    /**
     * Trim memory
     */
    @JvmStatic
    external fun nativeTrimMemory(level: Int)

    /**
     * Get statistics as JSON string
     */
    @JvmStatic
    external fun nativeGetStats(): String

    /**
     * Set quality preset
     */
    @JvmStatic
    external fun nativeSetQuality(quality: Int)

    /**
     * Enable/disable dynamic resolution
     */
    @JvmStatic
    external fun nativeSetDynamicResolution(enabled: Boolean)

    /**
     * Flush shader cache to disk
     */
    @JvmStatic
    external fun nativeFlushShaderCache()

    /**
     * Clear shader cache
     */
    @JvmStatic
    external fun nativeClearShaderCache()

    /**
     * Get GPU info as JSON string
     */
    @JvmStatic
    external fun nativeGetGPUInfo(): String

    // ========================================================================
    // Kotlin wrapper methods
    // ========================================================================

    private var initialized = false

    /**
     * Initialize VelocityGL
     */
    fun init(configPath: String? = null): Boolean {
        if (!VelocityGLApp.isLibraryLoaded) {
            return false
        }
        
        if (initialized) {
            return true
        }
        
        val path = configPath ?: VelocityGLApp.cacheDir.absolutePath
        initialized = nativeInit(path)
        return initialized
    }

    /**
     * Shutdown VelocityGL
     */
    fun shutdown() {
        if (initialized) {
            nativeShutdown()
            initialized = false
        }
    }

    /**
     * Create context from Surface
     */
    fun createContext(surface: Surface, eglDisplay: Long = 0L): Boolean {
        if (!initialized && !init()) {
            return false
        }
        return nativeCreateContext(surface, eglDisplay)
    }

    /**
     * Destroy context
     */
    fun destroyContext() {
        if (initialized) {
            nativeDestroyContext()
        }
    }

    /**
     * Swap buffers
     */
    fun swapBuffers() {
        if (initialized) {
            nativeSwapBuffers()
        }
    }

    /**
     * Get GL proc address
     */
    fun getProcAddress(name: String): Long {
        return if (initialized) nativeGetProcAddress(name) else 0L
    }

    /**
     * Get current FPS
     */
    fun getFPS(): Float {
        return if (initialized) nativeGetFPS() else 0f
    }

    /**
     * Set resolution scale (0.25 - 2.0)
     */
    fun setResolutionScale(scale: Float) {
        if (initialized) {
            nativeSetResolutionScale(scale.coerceIn(0.25f, 2.0f))
        }
    }

    /**
     * Get resolution scale
     */
    fun getResolutionScale(): Float {
        return if (initialized) nativeGetResolutionScale() else 1.0f
    }

    /**
     * Trim memory
     */
    fun trimMemory(level: Int) {
        if (initialized) {
            nativeTrimMemory(level)
        }
    }

    /**
     * Get renderer statistics
     */
    fun getStats(): RendererStats? {
        if (!initialized) return null
        
        return try {
            val json = nativeGetStats()
            RendererStats.fromJson(json)
        } catch (e: Exception) {
            null
        }
    }

    /**
     * Set quality preset
     */
    fun setQuality(quality: QualityPreset) {
        if (initialized) {
            nativeSetQuality(quality.ordinal)
        }
    }

    /**
     * Enable/disable dynamic resolution
     */
    fun setDynamicResolution(enabled: Boolean) {
        if (initialized) {
            nativeSetDynamicResolution(enabled)
        }
    }

    /**
     * Flush shader cache
     */
    fun flushShaderCache() {
        if (initialized) {
            nativeFlushShaderCache()
        }
    }

    /**
     * Clear shader cache
     */
    fun clearShaderCache() {
        if (initialized) {
            nativeClearShaderCache()
        }
    }

    /**
     * Get GPU info
     */
    fun getGPUInfo(): GPUInfo? {
        if (!initialized) return null
        
        return try {
            val json = nativeGetGPUInfo()
            GPUInfo.fromJson(json)
        } catch (e: Exception) {
            null
        }
    }

    /**
     * Check if initialized
     */
    fun isInitialized(): Boolean = initialized

    /**
     * Get library path for launcher integration
     */
    fun getLibraryPath(): String? {
        return try {
            val context = VelocityGLApp.getAppContext()
            val nativeLibDir = context.applicationInfo.nativeLibraryDir
            "$nativeLibDir/libvelocitygl.so"
        } catch (e: Exception) {
            null
        }
    }
}

/**
 * Quality presets
 */
enum class QualityPreset {
    ULTRA_LOW,
    LOW,
    MEDIUM,
    HIGH,
    ULTRA
}

/**
 * Renderer statistics
 */
data class RendererStats(
    val fps: Float,
    val frameTimeMs: Float,
    val drawCalls: Int,
    val triangles: Int,
    val textureMemoryMB: Float,
    val bufferMemoryMB: Float,
    val shaderCacheHits: Int,
    val shaderCacheMisses: Int,
    val resolutionScale: Float,
    val renderWidth: Int,
    val renderHeight: Int
) {
    companion object {
        fun fromJson(json: String): RendererStats {
            val map = parseSimpleJson(json)
            return RendererStats(
                fps = (map["fps"] as? Number)?.toFloat() ?: 0f,
                frameTimeMs = (map["frameTimeMs"] as? Number)?.toFloat() ?: 0f,
                drawCalls = (map["drawCalls"] as? Number)?.toInt() ?: 0,
                triangles = (map["triangles"] as? Number)?.toInt() ?: 0,
                textureMemoryMB = (map["textureMemoryMB"] as? Number)?.toFloat() ?: 0f,
                bufferMemoryMB = (map["bufferMemoryMB"] as? Number)?.toFloat() ?: 0f,
                shaderCacheHits = (map["shaderCacheHits"] as? Number)?.toInt() ?: 0,
                shaderCacheMisses = (map["shaderCacheMisses"] as? Number)?.toInt() ?: 0,
                resolutionScale = (map["resolutionScale"] as? Number)?.toFloat() ?: 1f,
                renderWidth = (map["renderWidth"] as? Number)?.toInt() ?: 0,
                renderHeight = (map["renderHeight"] as? Number)?.toInt() ?: 0
            )
        }
        
        // Changed to public/internal so GPUInfo can access it
        internal fun parseSimpleJson(json: String): Map<String, Any> {
            val result = mutableMapOf<String, Any>()
            val content = json.trim().removeSurrounding("{", "}")
            
            val pairs = content.split(",")
            for (pair in pairs) {
                val parts = pair.split(":", limit = 2)
                if (parts.size == 2) {
                    val key = parts[0].trim().removeSurrounding("\"")
                    val value = parts[1].trim()
                    
                    result[key] = when {
                        value == "true" -> true
                        value == "false" -> false
                        value.startsWith("\"") -> value.removeSurrounding("\"")
                        value.contains(".") -> value.toDoubleOrNull() ?: 0.0
                        else -> value.toIntOrNull() ?: 0
                    }
                }
            }
            
            return result
        }
    }
}

/**
 * GPU Information
 */
data class GPUInfo(
    val vendor: String,
    val renderer: String,
    val version: String,
    val glVersion: String,
    val maxTextureSize: Int,
    val hasComputeShaders: Boolean,
    val hasGeometryShaders: Boolean
) {
    companion object {
        fun fromJson(json: String): GPUInfo {
            // Using internal method from RendererStats
            val map = RendererStats.parseSimpleJson(json)
            return GPUInfo(
                vendor = map["vendor"] as? String ?: "Unknown",
                renderer = map["renderer"] as? String ?: "Unknown",
                version = map["version"] as? String ?: "Unknown",
                glVersion = map["glVersion"] as? String ?: "4.5",
                maxTextureSize = (map["maxTextureSize"] as? Number)?.toInt() ?: 4096,
                hasComputeShaders = map["hasComputeShaders"] as? Boolean ?: false,
                hasGeometryShaders = map["hasGeometryShaders"] as? Boolean ?: false
            )
        }
    }
}
