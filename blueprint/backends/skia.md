# Backend Integration: Skia

## Overview
Skia is a complete 2D graphic library for drawing Text, Geometries, and Images. It is the graphics engine for Chrome, Chrome OS, Android, Flutter, and Firefox.

## CPU-Only Configuration

### Surface Creation
To ensure CPU-only rendering (software rasterization):
1.  **Do not** initialize a backend context (like GrContext for OpenGL/Vulkan).
2.  Use `SkSurface::MakeRaster` or `SkSurface::MakeRasterDirect`.

```cpp
// Example: Creating a CPU-backed surface wrapping an existing buffer
SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
// OR specific format:
// SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);

// If we own the memory:
std::vector<uint8_t> buffer(width * height * 4);
sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(info, buffer.data(), width * 4);

// If we want Skia to manage memory:
sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
```

### Build Instructions (CMake Integration)
Skia uses GN (Generate Ninja) for its build system, which is hard to integrate directly into CMake. We recommend:
1.  **Prebuilding** Skia binaries using a script.
2.  **Linking** standard static libraries: `libskia.a` (or .lib).

**Recommended GN Args (`m116`)**:
```bash
is_official_build = true
skia_use_system_expat = false
skia_use_system_icu = false
skia_use_system_libjpeg_turbo = false
skia_use_system_libpng = false
skia_use_system_libwebp = false
skia_use_system_zlib = false
skia_use_gl = false 
skia_use_vulkan = false
skia_use_metal = false
skia_enable_pdf = false
skia_enable_skottie = false
skia_enable_sksl = false # If no GPU, SkSL heavily reduced
```

## Mapping Concepts

| Benchmark IR | Skia C++ API |
|---|---|
| `Path` | `SkPath` |
| `Paint` | `SkPaint` |
| `Matrix` | `SkMatrix` |
| `FillPath` | `canvas->drawPath(path, paint)` |
| `SetFill` | `paint.setStyle(SkPaint::kFill_Style)` |
| `SetStroke` | `paint.setStyle(SkPaint::kStroke_Style); paint.setStrokeWidth(...)` |

## Notes
*   **Premultiplication**: Skia is heavily optimized for Premultiplied Alpha. `SkImageInfo::MakeN32Premul` is the standard. If our IR provides non-premul, we must convert/premultiply before or during paint setup.
