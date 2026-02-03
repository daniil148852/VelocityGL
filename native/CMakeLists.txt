cmake_minimum_required(VERSION 3.22.1)

project(VelocityGL 
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "High-performance OpenGL wrapper for Android"
)

# ============================================================================
# Build Options
# ============================================================================
option(VELOCITY_ENABLE_VULKAN "Enable Vulkan backend via ANGLE" OFF)
option(VELOCITY_ENABLE_PROFILING "Enable GPU profiling" ON)
option(VELOCITY_SHADER_CACHE "Enable shader binary caching" ON)
option(VELOCITY_DEBUG_LOG "Enable debug logging" OFF)

# ============================================================================
# Compiler Settings
# ============================================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive optimizations for release
set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -fno-exceptions -fvisibility=hidden -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-rtti")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DVELOCITY_DEBUG=1")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

# Architecture specific optimizations
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+simd")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon -mfloat-abi=softfp")
endif()

# ============================================================================
# Source Files
# ============================================================================
set(VELOCITY_CORE_SOURCES
    src/core/gl_wrapper.c
    src/core/gl_context.c
    src/core/gl_state.c
    src/core/gl_extensions.c
    src/core/gl_caps.c
)

set(VELOCITY_SHADER_SOURCES
    src/shader/shader_cache.c
    src/shader/shader_translator.c
    src/shader/shader_optimizer.c
    src/shader/glsl_parser.c
)

set(VELOCITY_TEXTURE_SOURCES
    src/texture/texture_manager.c
    src/texture/texture_cache.c
    src/texture/texture_compress.c
    src/texture/async_loader.c
)

set(VELOCITY_BUFFER_SOURCES
    src/buffer/buffer_pool.c
    src/buffer/vao_cache.c
    src/buffer/uniform_buffer.c
    src/buffer/draw_batcher.c
)

set(VELOCITY_OPTIMIZE_SOURCES
    src/optimize/draw_optimizer.c
    src/optimize/state_optimizer.c
    src/optimize/resolution_scaler.c
    src/optimize/frame_pacing.c
)

set(VELOCITY_GPU_SOURCES
    src/gpu/gpu_detect.c
    src/gpu/adreno_tweaks.c
    src/gpu/mali_tweaks.c
    src/gpu/powervr_tweaks.c
)

set(VELOCITY_GL_FUNCTIONS
    src/gl/gl_draw.c
    src/gl/gl_texture.c
    src/gl/gl_shader.c
    src/gl/gl_buffer.c
    src/gl/gl_framebuffer.c
    src/gl/gl_query.c
    src/gl/gl_sync.c
    src/gl/gl_compute.c
)

set(VELOCITY_UTILS_SOURCES
    src/utils/log.c
    src/utils/hash.c
    src/utils/memory.c
    src/utils/thread_pool.c
    src/utils/config.c
)

# All sources combined
set(VELOCITY_ALL_SOURCES
    ${VELOCITY_CORE_SOURCES}
    ${VELOCITY_SHADER_SOURCES}
    ${VELOCITY_TEXTURE_SOURCES}
    ${VELOCITY_BUFFER_SOURCES}
    ${VELOCITY_OPTIMIZE_SOURCES}
    ${VELOCITY_GPU_SOURCES}
    ${VELOCITY_GL_FUNCTIONS}
    ${VELOCITY_UTILS_SOURCES}
    src/velocity_main.c
)

# ============================================================================
# Include Directories
# ============================================================================
set(VELOCITY_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GL
    ${CMAKE_CURRENT_SOURCE_DIR}/include/KHR
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Library Target
# ============================================================================
add_library(velocitygl SHARED ${VELOCITY_ALL_SOURCES})

target_include_directories(velocitygl PRIVATE ${VELOCITY_INCLUDE_DIRS})

# Find required libraries
find_library(GLES3_LIB GLESv3)
find_library(EGL_LIB EGL)
find_library(ANDROID_LIB android)
find_library(LOG_LIB log)
find_library(Z_LIB z)

target_link_libraries(velocitygl
    ${GLES3_LIB}
    ${EGL_LIB}
    ${ANDROID_LIB}
    ${LOG_LIB}
    ${Z_LIB}
)

# Compile definitions
target_compile_definitions(velocitygl PRIVATE
    VELOCITY_VERSION="${PROJECT_VERSION}"
    VELOCITY_GLES3=1
    $<$<BOOL:${VELOCITY_ENABLE_VULKAN}>:VELOCITY_VULKAN=1>
    $<$<BOOL:${VELOCITY_ENABLE_PROFILING}>:VELOCITY_PROFILING=1>
    $<$<BOOL:${VELOCITY_SHADER_CACHE}>:VELOCITY_SHADER_CACHE=1>
    $<$<BOOL:${VELOCITY_DEBUG_LOG}>:VELOCITY_DEBUG_LOG=1>
)

# Strip symbols in release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET velocitygl POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:velocitygl>
        COMMENT "Stripping library symbols"
    )
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS velocitygl
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
