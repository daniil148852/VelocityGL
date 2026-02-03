package com.velocitygl.plugin

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.Log
import com.velocitygl.VelocityGL
import com.velocitygl.VelocityGLApp

/**
 * Broadcast Receiver for launcher queries
 * Handles renderer discovery broadcasts from PojavLauncher, Zalith, etc.
 */
class RendererReceiver : BroadcastReceiver() {

    companion object {
        private const val TAG = "VelocityGL.Receiver"
        
        // Actions we respond to
        private const val ACTION_POJAV_QUERY = "net.kdt.pojavlaunch.RENDERER_QUERY"
        private const val ACTION_ZALITH_QUERY = "com.movtery.zalern.RENDERER_QUERY"
        private const val ACTION_GENERIC_QUERY = "com.velocitygl.RENDERER_QUERY"
        
        // Response actions
        private const val ACTION_POJAV_RESPONSE = "net.kdt.pojavlaunch.RENDERER_RESPONSE"
        private const val ACTION_ZALITH_RESPONSE = "com.movtery.zalern.RENDERER_RESPONSE"
        private const val ACTION_GENERIC_RESPONSE = "com.velocitygl.RENDERER_RESPONSE"
        
        // Extra keys
        private const val EXTRA_RENDERER_NAME = "renderer_name"
        private const val EXTRA_RENDERER_VERSION = "renderer_version"
        private const val EXTRA_GL_VERSION = "gl_version"
        private const val EXTRA_LIBRARY_PATH = "library_path"
        private const val EXTRA_LIBRARY_NAME = "library_name"
        private const val EXTRA_PACKAGE_NAME = "package_name"
        private const val EXTRA_PROVIDER_AUTHORITY = "provider_authority"
        private const val EXTRA_FEATURES = "features"
        private const val EXTRA_MIN_SDK = "min_sdk"
        private const val EXTRA_DESCRIPTION = "description"
    }

    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: return
        
        Log.d(TAG, "Received broadcast: $action")
        
        when (action) {
            ACTION_POJAV_QUERY -> respondToPojavLauncher(context, intent)
            ACTION_ZALITH_QUERY -> respondToZalith(context, intent)
            ACTION_GENERIC_QUERY -> respondGeneric(context, intent)
        }
    }

    /**
     * Respond to PojavLauncher query
     */
    private fun respondToPojavLauncher(context: Context, queryIntent: Intent) {
        val responseIntent = Intent(ACTION_POJAV_RESPONSE).apply {
            setPackage(queryIntent.getStringExtra("caller_package") ?: "net.kdt.pojavlaunch")
            putExtras(createRendererBundle(context))
            
            // PojavLauncher specific extras
            putExtra("renderer_type", "gl4es_like")
            putExtra("supports_shader_mods", true)
            putExtra("supports_sodium", true)
            putExtra("supports_iris", true)
        }
        
        context.sendBroadcast(responseIntent)
        Log.d(TAG, "Sent response to PojavLauncher")
    }

    /**
     * Respond to Zalith launcher query
     */
    private fun respondToZalith(context: Context, queryIntent: Intent) {
        val responseIntent = Intent(ACTION_ZALITH_RESPONSE).apply {
            setPackage(queryIntent.getStringExtra("caller_package") ?: "com.movtery.zalern")
            putExtras(createRendererBundle(context))
            
            // Zalith specific extras
            putExtra("renderer_id", "velocitygl")
            putExtra("priority", 100)  // Higher priority = preferred
        }
        
        context.sendBroadcast(responseIntent)
        Log.d(TAG, "Sent response to Zalith")
    }

    /**
     * Generic response
     */
    private fun respondGeneric(context: Context, queryIntent: Intent) {
        val callerPackage = queryIntent.getStringExtra("caller_package")
        
        val responseIntent = Intent(ACTION_GENERIC_RESPONSE).apply {
            if (callerPackage != null) {
                setPackage(callerPackage)
            }
            putExtras(createRendererBundle(context))
        }
        
        context.sendBroadcast(responseIntent)
        Log.d(TAG, "Sent generic response")
    }

    /**
     * Create bundle with renderer information
     */
    private fun createRendererBundle(context: Context): Bundle {
        val libraryPath = "${context.applicationInfo.nativeLibraryDir}/libvelocitygl.so"
        
        return Bundle().apply {
            putString(EXTRA_RENDERER_NAME, "VelocityGL")
            putString(EXTRA_RENDERER_VERSION, "1.0.0")
            putString(EXTRA_GL_VERSION, "4.5")
            putString(EXTRA_LIBRARY_PATH, libraryPath)
            putString(EXTRA_LIBRARY_NAME, "libvelocitygl.so")
            putString(EXTRA_PACKAGE_NAME, context.packageName)
            putString(EXTRA_PROVIDER_AUTHORITY, "${context.packageName}.renderer")
            putInt(EXTRA_MIN_SDK, 26)
            putString(EXTRA_DESCRIPTION, "High-performance OpenGL 4.5 renderer for Minecraft")
            
            // Feature flags
            putStringArrayList(EXTRA_FEATURES, arrayListOf(
                "shader_cache",
                "dynamic_resolution", 
                "draw_batching",
                "gpu_optimization",
                "async_texture_loading",
                "sodium_compatible",
                "iris_compatible",
                "compute_shaders"
            ))
            
            // Additional info
            putBoolean("is_loaded", VelocityGLApp.isLibraryLoaded)
            putBoolean("is_initialized", VelocityGL.isInitialized())
        }
    }
}
