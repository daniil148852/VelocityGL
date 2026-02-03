package com.velocitygl.plugin

import android.content.ContentProvider
import android.content.ContentValues
import android.content.UriMatcher
import android.database.Cursor
import android.database.MatrixCursor
import android.net.Uri
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.util.Log
import com.velocitygl.VelocityGL
import com.velocitygl.VelocityGLApp
import java.io.File
import java.io.FileNotFoundException

/**
 * Content Provider for launcher integration
 * Allows launchers like PojavLauncher to discover and use VelocityGL
 */
class RendererProvider : ContentProvider() {

    companion object {
        private const val TAG = "VelocityGL.Provider"
        
        private const val AUTHORITY = "com.velocitygl.renderer.renderer"
        
        // URI codes
        private const val CODE_INFO = 1
        private const val CODE_LIBRARY = 2
        private const val CODE_CONFIG = 3
        private const val CODE_PROC = 4
        
        private val uriMatcher = UriMatcher(UriMatcher.NO_MATCH).apply {
            addURI(AUTHORITY, "info", CODE_INFO)
            addURI(AUTHORITY, "library", CODE_LIBRARY)
            addURI(AUTHORITY, "config", CODE_CONFIG)
            addURI(AUTHORITY, "proc/*", CODE_PROC)
        }
        
        // Renderer info columns
        val INFO_COLUMNS = arrayOf(
            "name",
            "version",
            "gl_version",
            "library_path",
            "library_name",
            "description",
            "author",
            "features"
        )
    }

    override fun onCreate(): Boolean {
        Log.d(TAG, "RendererProvider created")
        return true
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? {
        return when (uriMatcher.match(uri)) {
            CODE_INFO -> queryInfo(projection)
            CODE_CONFIG -> queryConfig()
            else -> null
        }
    }

    /**
     * Query renderer information
     */
    private fun queryInfo(projection: Array<out String>?): Cursor {
        val columns = projection ?: INFO_COLUMNS
        val cursor = MatrixCursor(columns)
        
        val context = context ?: return cursor
        val libraryPath = "${context.applicationInfo.nativeLibraryDir}/libvelocitygl.so"
        
        val row = columns.map { column ->
            when (column) {
                "name" -> "VelocityGL"
                "version" -> "1.0.0"
                "gl_version" -> "4.5"
                "library_path" -> libraryPath
                "library_name" -> "libvelocitygl.so"
                "description" -> "High-performance OpenGL renderer with FPS optimizations"
                "author" -> "VelocityGL Team"
                "features" -> "shader_cache,dynamic_resolution,draw_batching,gpu_tweaks"
                else -> null
            }
        }.toTypedArray()
        
        cursor.addRow(row)
        return cursor
    }

    /**
     * Query configuration
     */
    private fun queryConfig(): Cursor {
        val columns = arrayOf("key", "value", "type")
        val cursor = MatrixCursor(columns)
        
        // Add config entries
        val configs = listOf(
            Triple("quality", "2", "int"),
            Triple("dynamic_resolution", "true", "bool"),
            Triple("target_fps", "60", "int"),
            Triple("shader_cache", "true", "bool")
        )
        
        configs.forEach { (key, value, type) ->
            cursor.addRow(arrayOf(key, value, type))
        }
        
        return cursor
    }

    override fun getType(uri: Uri): String? {
        return when (uriMatcher.match(uri)) {
            CODE_INFO -> "vnd.android.cursor.item/vnd.velocitygl.info"
            CODE_LIBRARY -> "application/octet-stream"
            CODE_CONFIG -> "vnd.android.cursor.dir/vnd.velocitygl.config"
            else -> null
        }
    }

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor? {
        if (uriMatcher.match(uri) == CODE_LIBRARY) {
            val context = context ?: throw FileNotFoundException("Context not available")
            val libraryPath = "${context.applicationInfo.nativeLibraryDir}/libvelocitygl.so"
            val file = File(libraryPath)
            
            if (!file.exists()) {
                throw FileNotFoundException("Library not found: $libraryPath")
            }
            
            return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
        }
        
        throw FileNotFoundException("Unknown URI: $uri")
    }

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle? {
        val result = Bundle()
        
        when (method) {
            "getLibraryPath" -> {
                val context = context ?: return null
                result.putString("path", "${context.applicationInfo.nativeLibraryDir}/libvelocitygl.so")
            }
            
            "getProcAddress" -> {
                if (arg != null && VelocityGLApp.isLibraryLoaded) {
                    val address = VelocityGL.getProcAddress(arg)
                    result.putLong("address", address)
                }
            }
            
            "getRendererInfo" -> {
                result.putString("name", "VelocityGL")
                result.putString("version", "1.0.0")
                result.putString("gl_version", "4.5")
                result.putBoolean("initialized", VelocityGL.isInitialized())
            }
            
            "init" -> {
                val configPath = extras?.getString("configPath")
                val success = VelocityGL.init(configPath)
                result.putBoolean("success", success)
            }
            
            "shutdown" -> {
                VelocityGL.shutdown()
                result.putBoolean("success", true)
            }
            
            else -> return null
        }
        
        return result
    }

    // Not implemented - read only provider
    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?): Int = 0
}
