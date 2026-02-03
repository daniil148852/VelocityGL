package com.velocitygl

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import java.io.File

/**
 * Configuration Manager
 * Handles loading, saving, and managing VelocityGL settings
 */
object ConfigManager {

    private const val TAG = "VelocityGL.Config"
    private const val PREFS_NAME = "velocitygl_prefs"
    private const val CONFIG_FILE = "config.json"

    private lateinit var prefs: SharedPreferences
    private lateinit var configFile: File
    private val gson: Gson = GsonBuilder().setPrettyPrinting().create()

    private var currentConfig: VelocityConfig = VelocityConfig.default()

    /**
     * Initialize config manager
     */
    fun init(context: Context) {
        prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        configFile = File(VelocityGLApp.configDir, CONFIG_FILE)
        
        // Load config
        loadConfig()
        
        Log.d(TAG, "ConfigManager initialized")
    }

    /**
     * Get current configuration
     */
    fun getConfig(): VelocityConfig = currentConfig.copy()

    /**
     * Update configuration
     */
    fun updateConfig(config: VelocityConfig) {
        currentConfig = config.copy()
        saveConfig()
        
        // Apply to native if initialized
        applyToNative()
    }

    /**
     * Load configuration from file
     */
    private fun loadConfig() {
        try {
            if (configFile.exists()) {
                val json = configFile.readText()
                currentConfig = gson.fromJson(json, VelocityConfig::class.java)
                Log.d(TAG, "Config loaded from file")
            } else {
                // Try to load from SharedPreferences for migration
                loadFromPrefs()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load config: ${e.message}")
            currentConfig = VelocityConfig.default()
        }
    }

    /**
     * Save configuration to file
     */
    private fun saveConfig() {
        try {
            val json = gson.toJson(currentConfig)
            configFile.writeText(json)
            
            // Also save key values to prefs for quick access
            saveToPrefs()
            
            Log.d(TAG, "Config saved")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save config: ${e.message}")
        }
    }

    /**
     * Load from SharedPreferences (migration)
     */
    private fun loadFromPrefs() {
        currentConfig = VelocityConfig(
            quality = QualityPreset.values()[prefs.getInt("quality", QualityPreset.MEDIUM.ordinal)],
            enableDynamicResolution = prefs.getBoolean("dynamic_resolution", true),
            minResolutionScale = prefs.getFloat("min_scale", 0.5f),
            maxResolutionScale = prefs.getFloat("max_scale", 1.0f),
            targetFPS = prefs.getInt("target_fps", 60),
            enableDrawBatching = prefs.getBoolean("draw_batching", true),
            enableInstancing = prefs.getBoolean("instancing", true),
            enableShaderCache = prefs.getBoolean("shader_cache", true),
            enableGPUTweaks = prefs.getBoolean("gpu_tweaks", true),
            textureQuality = prefs.getInt("texture_quality", 2),
            maxTextureSize = prefs.getInt("max_texture_size", 4096),
            anisotropicFiltering = prefs.getInt("anisotropic", 4),
            enableDebugOverlay = prefs.getBoolean("debug_overlay", false)
        )
    }

    /**
     * Save to SharedPreferences
     */
    private fun saveToPrefs() {
        prefs.edit().apply {
            putInt("quality", currentConfig.quality.ordinal)
            putBoolean("dynamic_resolution", currentConfig.enableDynamicResolution)
            putFloat("min_scale", currentConfig.minResolutionScale)
            putFloat("max_scale", currentConfig.maxResolutionScale)
            putInt("target_fps", currentConfig.targetFPS)
            putBoolean("draw_batching", currentConfig.enableDrawBatching)
            putBoolean("instancing", currentConfig.enableInstancing)
            putBoolean("shader_cache", currentConfig.enableShaderCache)
            putBoolean("gpu_tweaks", currentConfig.enableGPUTweaks)
            putInt("texture_quality", currentConfig.textureQuality)
            putInt("max_texture_size", currentConfig.maxTextureSize)
            putInt("anisotropic", currentConfig.anisotropicFiltering)
            putBoolean("debug_overlay", currentConfig.enableDebugOverlay)
            apply()
        }
    }

    /**
     * Apply config to native library
     */
    private fun applyToNative() {
        if (!VelocityGL.isInitialized()) return
        
        VelocityGL.setQuality(currentConfig.quality)
        VelocityGL.setDynamicResolution(currentConfig.enableDynamicResolution)
    }

    /**
     * Reset to defaults
     */
    fun resetToDefaults() {
        currentConfig = VelocityConfig.default()
        saveConfig()
        applyToNative()
    }

    /**
     * Apply preset
     */
    fun applyPreset(preset: QualityPreset) {
        currentConfig = VelocityConfig.forPreset(preset)
        saveConfig()
        applyToNative()
    }

    /**
     * Export config to path
     */
    fun exportConfig(path: File): Boolean {
        return try {
            val json = gson.toJson(currentConfig)
            path.writeText(json)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export config: ${e.message}")
            false
        }
    }

    /**
     * Import config from path
     */
    fun importConfig(path: File): Boolean {
        return try {
            val json = path.readText()
            currentConfig = gson.fromJson(json, VelocityConfig::class.java)
            saveConfig()
            applyToNative()
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to import config: ${e.message}")
            false
        }
    }

    /**
     * Get cache directory for native library
     */
    fun getCachePath(): String = VelocityGLApp.cacheDir.absolutePath

    /**
     * Get shader cache size in MB
     */
    fun getShaderCacheSize(): Float {
        val cacheDir = File(VelocityGLApp.cacheDir, "shaders")
        if (!cacheDir.exists()) return 0f
        
        var size = 0L
        cacheDir.walkTopDown().forEach { file ->
            if (file.isFile) size += file.length()
        }
        
        return size / (1024f * 1024f)
    }

    /**
     * Clear shader cache
     */
    fun clearShaderCache() {
        VelocityGL.clearShaderCache()
        
        val cacheDir = File(VelocityGLApp.cacheDir, "shaders")
        if (cacheDir.exists()) {
            cacheDir.deleteRecursively()
        }
    }
}

/**
 * VelocityGL Configuration data class
 */
data class VelocityConfig(
    val quality: QualityPreset = QualityPreset.MEDIUM,
    val enableDynamicResolution: Boolean = true,
    val minResolutionScale: Float = 0.5f,
    val maxResolutionScale: Float = 1.0f,
    val targetFPS: Int = 60,
    val enableDrawBatching: Boolean = true,
    val enableInstancing: Boolean = true,
    val enableShaderCache: Boolean = true,
    val enableGPUTweaks: Boolean = true,
    val textureQuality: Int = 2,  // 0=Low, 1=Medium, 2=High, 3=Ultra
    val maxTextureSize: Int = 4096,
    val anisotropicFiltering: Int = 4,
    val enableDebugOverlay: Boolean = false
) {
    companion object {
        fun default() = VelocityConfig()
        
        fun forPreset(preset: QualityPreset): VelocityConfig {
            return when (preset) {
                QualityPreset.ULTRA_LOW -> VelocityConfig(
                    quality = preset,
                    minResolutionScale = 0.25f,
                    maxResolutionScale = 0.5f,
                    targetFPS = 30,
                    enableInstancing = false,
                    textureQuality = 0,
                    maxTextureSize = 1024,
                    anisotropicFiltering = 1
                )
                QualityPreset.LOW -> VelocityConfig(
                    quality = preset,
                    minResolutionScale = 0.4f,
                    maxResolutionScale = 0.7f,
                    targetFPS = 30,
                    textureQuality = 1,
                    maxTextureSize = 2048,
                    anisotropicFiltering = 2
                )
                QualityPreset.MEDIUM -> VelocityConfig(
                    quality = preset,
                    minResolutionScale = 0.5f,
                    maxResolutionScale = 1.0f,
                    targetFPS = 45,
                    textureQuality = 2,
                    maxTextureSize = 4096,
                    anisotropicFiltering = 4
                )
                QualityPreset.HIGH -> VelocityConfig(
                    quality = preset,
                    minResolutionScale = 0.7f,
                    maxResolutionScale = 1.0f,
                    targetFPS = 60,
                    textureQuality = 2,
                    maxTextureSize = 4096,
                    anisotropicFiltering = 8
                )
                QualityPreset.ULTRA -> VelocityConfig(
                    quality = preset,
                    enableDynamicResolution = false,
                    minResolutionScale = 1.0f,
                    maxResolutionScale = 1.0f,
                    targetFPS = 60,
                    textureQuality = 3,
                    maxTextureSize = 8192,
                    anisotropicFiltering = 16
                )
            }
        }
    }
}
