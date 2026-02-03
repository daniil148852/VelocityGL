package com.velocitygl.ui

import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.*
import com.velocitygl.*
import com.velocitygl.databinding.ActivitySettingsBinding
// Ensure R is imported correctly
import com.velocitygl.R

/**
 * Settings Activity - Detailed configuration
 */
class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        setSupportActionBar(binding.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = "Settings"
        
        if (savedInstanceState == null) {
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settingsContainer, SettingsFragment())
                .commit()
        }
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressedDispatcher.onBackPressed()
        return true
    }

    /**
     * Settings Fragment
     */
    class SettingsFragment : PreferenceFragmentCompat() {

        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)
            
            setupPreferences()
            loadCurrentValues()
        }

        private fun setupPreferences() {
            // ... (rest of the file remains the same)
            // Just ensure R references are resolved
            
            // Quality preset
            findPreference<ListPreference>("pref_quality")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    val preset = QualityPreset.values()[(newValue as String).toInt()]
                    ConfigManager.applyPreset(preset)
                    loadCurrentValues()
                    true
                }
            }

            // Dynamic resolution
            findPreference<SwitchPreferenceCompat>("pref_dynamic_resolution")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableDynamicResolution = newValue as Boolean) }
                    true
                }
            }

            // Min resolution scale
            findPreference<SeekBarPreference>("pref_min_resolution")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    val scale = (newValue as Int) / 100f
                    updateConfig { it.copy(minResolutionScale = scale) }
                    true
                }
            }

            // Max resolution scale
            findPreference<SeekBarPreference>("pref_max_resolution")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    val scale = (newValue as Int) / 100f
                    updateConfig { it.copy(maxResolutionScale = scale) }
                    true
                }
            }

            // Target FPS
            findPreference<SeekBarPreference>("pref_target_fps")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(targetFPS = newValue as Int) }
                    true
                }
            }

            // Draw batching
            findPreference<SwitchPreferenceCompat>("pref_draw_batching")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableDrawBatching = newValue as Boolean) }
                    true
                }
            }

            // Instancing
            findPreference<SwitchPreferenceCompat>("pref_instancing")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableInstancing = newValue as Boolean) }
                    true
                }
            }

            // Shader cache
            findPreference<SwitchPreferenceCompat>("pref_shader_cache")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableShaderCache = newValue as Boolean) }
                    true
                }
            }

            // GPU tweaks
            findPreference<SwitchPreferenceCompat>("pref_gpu_tweaks")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableGPUTweaks = newValue as Boolean) }
                    true
                }
            }

            // Texture quality
            findPreference<ListPreference>("pref_texture_quality")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(textureQuality = (newValue as String).toInt()) }
                    true
                }
            }

            // Max texture size
            findPreference<ListPreference>("pref_max_texture_size")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(maxTextureSize = (newValue as String).toInt()) }
                    true
                }
            }

            // Anisotropic filtering
            findPreference<ListPreference>("pref_anisotropic")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(anisotropicFiltering = (newValue as String).toInt()) }
                    true
                }
            }

            // Debug overlay
            findPreference<SwitchPreferenceCompat>("pref_debug_overlay")?.apply {
                setOnPreferenceChangeListener { _, newValue ->
                    updateConfig { it.copy(enableDebugOverlay = newValue as Boolean) }
                    true
                }
            }

            // Clear shader cache
            findPreference<Preference>("pref_clear_cache")?.apply {
                setOnPreferenceClickListener {
                    ConfigManager.clearShaderCache()
                    Toast.makeText(context, "Shader cache cleared", Toast.LENGTH_SHORT).show()
                    updateCacheSummary()
                    true
                }
                updateCacheSummary()
            }

            // Reset to defaults
            findPreference<Preference>("pref_reset_defaults")?.apply {
                setOnPreferenceClickListener {
                    ConfigManager.resetToDefaults()
                    loadCurrentValues()
                    Toast.makeText(context, "Settings reset to defaults", Toast.LENGTH_SHORT).show()
                    true
                }
            }

            // GPU Info
            findPreference<Preference>("pref_gpu_info")?.apply {
                if (VelocityGL.isInitialized()) {
                    VelocityGL.getGPUInfo()?.let { gpu ->
                        summary = "${gpu.vendor}\n${gpu.renderer}\nGL ${gpu.glVersion}"
                    }
                } else {
                    summary = "Initialize VelocityGL to see GPU info"
                }
            }

            // Version info
            findPreference<Preference>("pref_version")?.apply {
                summary = "VelocityGL v1.0.0\nLibrary: ${if (VelocityGLApp.isLibraryLoaded) "Loaded" else "Not loaded"}"
            }
        }

        private fun loadCurrentValues() {
            val config = ConfigManager.getConfig()

            findPreference<ListPreference>("pref_quality")?.value = config.quality.ordinal.toString()
            findPreference<SwitchPreferenceCompat>("pref_dynamic_resolution")?.isChecked = config.enableDynamicResolution
            findPreference<SeekBarPreference>("pref_min_resolution")?.value = (config.minResolutionScale * 100).toInt()
            findPreference<SeekBarPreference>("pref_max_resolution")?.value = (config.maxResolutionScale * 100).toInt()
            findPreference<SeekBarPreference>("pref_target_fps")?.value = config.targetFPS
            findPreference<SwitchPreferenceCompat>("pref_draw_batching")?.isChecked = config.enableDrawBatching
            findPreference<SwitchPreferenceCompat>("pref_instancing")?.isChecked = config.enableInstancing
            findPreference<SwitchPreferenceCompat>("pref_shader_cache")?.isChecked = config.enableShaderCache
            findPreference<SwitchPreferenceCompat>("pref_gpu_tweaks")?.isChecked = config.enableGPUTweaks
            findPreference<ListPreference>("pref_texture_quality")?.value = config.textureQuality.toString()
            findPreference<ListPreference>("pref_max_texture_size")?.value = config.maxTextureSize.toString()
            findPreference<ListPreference>("pref_anisotropic")?.value = config.anisotropicFiltering.toString()
            findPreference<SwitchPreferenceCompat>("pref_debug_overlay")?.isChecked = config.enableDebugOverlay
        }

        private fun updateConfig(update: (VelocityConfig) -> VelocityConfig) {
            val current = ConfigManager.getConfig()
            val updated = update(current)
            ConfigManager.updateConfig(updated)
        }

        private fun updateCacheSummary() {
            findPreference<Preference>("pref_clear_cache")?.apply {
                val size = ConfigManager.getShaderCacheSize()
                summary = String.format("Current size: %.1f MB", size)
            }
        }
    }
}
