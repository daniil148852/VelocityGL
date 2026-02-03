package com.velocitygl.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.velocitygl.*
import com.velocitygl.databinding.ActivityMainBinding
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import com.velocitygl.R

/**
 * Main Activity - Dashboard and quick settings
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var statsUpdateJob: kotlinx.coroutines.Job? = null

    companion object {
        private const val PERMISSION_REQUEST_CODE = 1001
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        setupUI()
        checkPermissions()
        updateStatus()
    }

    override fun onResume() {
        super.onResume()
        updateStatus()
        startStatsUpdate()
    }

    override fun onPause() {
        super.onPause()
        statsUpdateJob?.cancel()
    }

    private fun setupUI() {
        // Toolbar
        setSupportActionBar(binding.toolbar)
        supportActionBar?.title = "VelocityGL"
        
        // Status card
        binding.cardStatus.setOnClickListener {
            if (!VelocityGLApp.isLibraryLoaded) {
                Toast.makeText(this, "Native library not loaded", Toast.LENGTH_SHORT).show()
            }
        }
        
        // Quick settings - Quality preset
        binding.chipGroupQuality.setOnCheckedStateChangeListener { group, checkedIds ->
            if (checkedIds.isNotEmpty()) {
                val preset = when (checkedIds.first()) {
                    R.id.chipLow -> QualityPreset.LOW
                    R.id.chipMedium -> QualityPreset.MEDIUM
                    R.id.chipHigh -> QualityPreset.HIGH
                    R.id.chipUltra -> QualityPreset.ULTRA
                    else -> QualityPreset.MEDIUM
                }
                ConfigManager.applyPreset(preset)
                Toast.makeText(this, "Applied ${preset.name} preset", Toast.LENGTH_SHORT).show()
            }
        }
        
        // Dynamic resolution toggle
        binding.switchDynamicRes.setOnCheckedChangeListener { _, isChecked ->
            val config = ConfigManager.getConfig()
            ConfigManager.updateConfig(config.copy(enableDynamicResolution = isChecked))
        }
        
        // Resolution scale slider
        binding.sliderResolution.addOnChangeListener { _, value, fromUser ->
            if (fromUser) {
                VelocityGL.setResolutionScale(value)
                binding.textResolutionValue.text = "${(value * 100).toInt()}%"
            }
        }
        
        // Target FPS
        binding.sliderTargetFps.addOnChangeListener { _, value, fromUser ->
            if (fromUser) {
                val config = ConfigManager.getConfig()
                ConfigManager.updateConfig(config.copy(targetFPS = value.toInt()))
                binding.textFpsValue.text = "${value.toInt()} FPS"
            }
        }
        
        // Settings button
        binding.btnSettings.setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }
        
        // Clear cache button
        binding.btnClearCache.setOnClickListener {
            ConfigManager.clearShaderCache()
            Toast.makeText(this, "Shader cache cleared", Toast.LENGTH_SHORT).show()
            updateCacheSize()
        }
        
        // Initialize button (for testing)
        binding.btnInitialize.setOnClickListener {
            if (VelocityGL.isInitialized()) {
                VelocityGL.shutdown()
                updateStatus()
            } else {
                val success = VelocityGL.init()
                updateStatus()
                Toast.makeText(
                    this,
                    if (success) "Initialized successfully" else "Initialization failed",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
        
        // Load current config values
        loadConfigValues()
    }

    private fun loadConfigValues() {
        val config = ConfigManager.getConfig()
        
        // Set quality chip
        val chipId = when (config.quality) {
            QualityPreset.ULTRA_LOW, QualityPreset.LOW -> R.id.chipLow
            QualityPreset.MEDIUM -> R.id.chipMedium
            QualityPreset.HIGH -> R.id.chipHigh
            QualityPreset.ULTRA -> R.id.chipUltra
        }
        binding.chipGroupQuality.check(chipId)
        
        // Dynamic resolution
        binding.switchDynamicRes.isChecked = config.enableDynamicResolution
        
        // Resolution scale
        binding.sliderResolution.value = config.maxResolutionScale
        binding.textResolutionValue.text = "${(config.maxResolutionScale * 100).toInt()}%"
        
        // Target FPS
        binding.sliderTargetFps.value = config.targetFPS.toFloat()
        binding.textFpsValue.text = "${config.targetFPS} FPS"
    }

    private fun updateStatus() {
        val isLoaded = VelocityGLApp.isLibraryLoaded
        val isInitialized = VelocityGL.isInitialized()
        
        // Status indicator
        binding.statusIndicator.setBackgroundColor(
            ContextCompat.getColor(
                this,
                when {
                    isInitialized -> android.R.color.holo_green_light
                    isLoaded -> android.R.color.holo_orange_light
                    else -> android.R.color.holo_red_light
                }
            )
        )
        
        binding.textStatus.text = when {
            isInitialized -> "Ready"
            isLoaded -> "Library Loaded"
            else -> "Not Loaded"
        }
        
        // Library path
        binding.textLibraryPath.text = VelocityGL.getLibraryPath() ?: "Not available"
        
        // GPU Info
        if (isInitialized) {
            VelocityGL.getGPUInfo()?.let { gpu ->
                binding.textGpuInfo.text = "${gpu.renderer}\n${gpu.glVersion}"
            }
        } else {
            binding.textGpuInfo.text = "Initialize to detect GPU"
        }
        
        // Init button text
        binding.btnInitialize.text = if (isInitialized) "Shutdown" else "Initialize"
        
        // Cache size
        updateCacheSize()
    }

    private fun updateCacheSize() {
        val cacheSize = ConfigManager.getShaderCacheSize()
        binding.textCacheSize.text = String.format("%.1f MB", cacheSize)
    }

    private fun startStatsUpdate() {
        statsUpdateJob = lifecycleScope.launch {
            while (isActive) {
                if (VelocityGL.isInitialized()) {
                    VelocityGL.getStats()?.let { stats ->
                        binding.textFps.text = String.format("%.1f FPS", stats.fps)
                        binding.textFrameTime.text = String.format("%.2f ms", stats.frameTimeMs)
                        binding.textDrawCalls.text = "${stats.drawCalls} calls"
                        binding.textTriangles.text = "${stats.triangles / 1000}K tris"
                        binding.textCurrentScale.text = "${(stats.resolutionScale * 100).toInt()}%"
                    }
                }
                delay(500)
            }
        }
    }

    private fun checkPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
                    intent.data = Uri.parse("package:$packageName")
                    startActivity(intent)
                } catch (e: Exception) {
                    val intent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                    startActivity(intent)
                }
            }
        } else {
            val permissions = arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            )
            
            val notGranted = permissions.filter {
                ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
            }
            
            if (notGranted.isNotEmpty()) {
                ActivityCompat.requestPermissions(
                    this,
                    notGranted.toTypedArray(),
                    PERMISSION_REQUEST_CODE
                )
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        if (requestCode == PERMISSION_REQUEST_CODE) {
            val allGranted = grantResults.all { it == PackageManager.PERMISSION_GRANTED }
            if (!allGranted) {
                Toast.makeText(
                    this,
                    "Storage permission required for shader cache",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }
}
