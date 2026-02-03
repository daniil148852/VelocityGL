# VelocityGL - High Performance OpenGL Renderer

Кастомный OpenGL 4.x → OpenGL ES 3.x рендерер для Minecraft Java Edition на Android.
Оптимизирован для максимального FPS с поддержкой Sodium, Iris и популярных модов.

## Особенности

- 🚀 **Shader Caching** - Бинарное кэширование скомпилированных шейдеров
- 📦 **Draw Call Batching** - Группировка вызовов отрисовки
- 🎮 **GPU-Specific Optimizations** - Оптимизации для Adreno, Mali, PowerVR
- 🔧 **Adaptive Resolution** - Динамическое масштабирование разрешения
- ⚡ **Async Loading** - Асинхронная загрузка текстур
- 🎨 **Vulkan Backend** - Опциональная поддержка через ANGLE/Zink

## Совместимость

- **Лаунчеры**: PojavLauncher, Zalith, Amethyst
- **Minecraft**: 1.7.10 - 1.21+
- **Моды**: Sodium, Iris, Create, JourneyMap, OptiFine
- **Android**: 8.0+ (API 26+)
- **GPU**: Adreno 5xx+, Mali-G71+, PowerVR GE8xxx+

## Структура проекта
