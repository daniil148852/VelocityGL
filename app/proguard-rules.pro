# VelocityGL ProGuard Rules

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep VelocityGL classes
-keep class com.velocitygl.VelocityGL { *; }
-keep class com.velocitygl.VelocityGLApp { *; }
-keep class com.velocitygl.RendererStats { *; }
-keep class com.velocitygl.GPUInfo { *; }
-keep class com.velocitygl.VelocityConfig { *; }
-keep class com.velocitygl.QualityPreset { *; }

# Keep plugin classes for launcher integration
-keep class com.velocitygl.plugin.** { *; }

# Keep JNI referenced classes
-keepclassmembers class * {
    @androidx.annotation.Keep *;
}

# Gson
-keepattributes Signature
-keepattributes *Annotation*
-keep class com.google.gson.** { *; }
-keep class * implements com.google.gson.TypeAdapterFactory
-keep class * implements com.google.gson.JsonSerializer
-keep class * implements com.google.gson.JsonDeserializer

# Keep data classes for serialization
-keepclassmembers class com.velocitygl.VelocityConfig {
    <fields>;
    <init>(...);
}

# Keep enum values
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# Kotlin
-keep class kotlin.Metadata { *; }
-keepclassmembers class kotlin.Metadata {
    public <methods>;
}

# Coroutines
-keepnames class kotlinx.coroutines.internal.MainDispatcherFactory {}
-keepnames class kotlinx.coroutines.CoroutineExceptionHandler {}
-keepclassmembers class kotlinx.coroutines.** {
    volatile <fields>;
}

# AndroidX
-keep class androidx.preference.** { *; }

# Remove logging in release
-assumenosideeffects class android.util.Log {
    public static int v(...);
    public static int d(...);
    public static int i(...);
}
