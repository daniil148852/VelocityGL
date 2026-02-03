package com.velocitygl

import android.app.Application
import android.content.Context
import android.util.Log
import java.io.File

/**
 * VelocityGL Application class
 * Handles initialization and global state
 */
class VelocityGLApp : Application() {

    companion object {
        private const val TAG = "VelocityGL"
        
        @Volatile
        private var instance: VelocityGLApp? = null
        
        fun getInstance(): VelocityGLApp = instance 
            ?: throw IllegalStateException("Application not initialized")
        
        fun getAppContext(): Context = getInstance().applicationContext
        
        // Library loading state
        @Volatile
        var isLibraryLoaded = false
            private set
        
        // Paths
        lateinit var configDir: File
            private set
        lateinit var cacheDir: File
            private set
        lateinit var logDir: File
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        
        Log.i(TAG, "VelocityGL Application starting...")
        
        // Setup directories
        setupDirectories()
        
        // Load native library
        loadNativeLibrary()
        
        // Initialize configuration
        ConfigManager.init(this)
        
        Log.i(TAG, "VelocityGL Application initialized")
    }

    private fun setupDirectories() {
        // Use external files dir for better accessibility
        val baseDir = getExternalFilesDir(null) ?: filesDir
        
        configDir = File(baseDir, "config").apply { mkdirs() }
        Companion.cacheDir = File(baseDir, "cache").apply { mkdirs() }
        logDir = File(baseDir, "logs").apply { mkdirs() }
        
        // Also create legacy path for compatibility
        val legacyDir = File("/sdcard/VelocityGL")
        if (!legacyDir.exists()) {
            try {
                legacyDir.mkdirs()
                File(legacyDir, "cache").mkdirs()
            } catch (e: Exception) {
                Log.w(TAG, "Could not create legacy directory: ${e.message}")
            }
        }
        
        Log.d(TAG, "Config dir: ${configDir.absolutePath}")
        Log.d(TAG, "Cache dir: ${Companion.cacheDir.absolutePath}")
    }

    private fun loadNativeLibrary() {
        try {
            System.loadLibrary("velocitygl")
            isLibraryLoaded = true
            Log.i(TAG, "Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library: ${e.message}")
            isLibraryLoaded = false
        }
    }

    override fun onTerminate() {
        super.onTerminate()
        
        if (isLibraryLoaded) {
            try {
                VelocityGL.shutdown()
            } catch (e: Exception) {
                Log.e(TAG, "Error during shutdown: ${e.message}")
            }
        }
    }

    override fun onLowMemory() {
        super.onLowMemory()
        Log.w(TAG, "Low memory warning received")
        
        if (isLibraryLoaded) {
            VelocityGL.trimMemory(2)
        }
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        
        if (isLibraryLoaded) {
            val trimLevel = when {
                level >= TRIM_MEMORY_COMPLETE -> 3
                level >= TRIM_MEMORY_MODERATE -> 2
                level >= TRIM_MEMORY_BACKGROUND -> 1
                else -> 0
            }
            
            if (trimLevel > 0) {
                Log.d(TAG, "Trimming memory (level: $trimLevel)")
                VelocityGL.trimMemory(trimLevel)
            }
        }
    }
}
