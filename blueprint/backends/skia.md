<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Skia (CPU Raster)

> **CPU-Only**: This backend uses Skia's `SkRasterPipeline` software rasterizer. GPU backends (OpenGL, Vulkan, Metal) are NOT in scope for this benchmark. For optimal CPU performance, **Clang is the recommended compiler**.

## 1. Version and Dependencies

*   **Version**: Milestone 116 (m116)
*   **Repository**: `https://skia.googlesource.com/skia.git`
*   **Commit Tag**: `chrome/m116`
*   **License**: BSD-3-Clause
*   **Rendering**: Pure CPU software rasterization via `SkSurface::MakeRasterDirect`

## 2. CMake Integration

Building Skia from source via CMake is difficult because it uses GN.
Recommended strategy: **Pre-build script** invoked by CMake, or `ExternalProject_Add`.

```cmake
# Pseudo-code for ExternalProject
ExternalProject_Add(
    skia_build
    GIT_REPOSITORY https://skia.googlesource.com/skia.git
    GIT_TAG        chrome/m116
    CONFIGURE_COMMAND bin/gn gen out/Release --args=...
    BUILD_COMMAND ninja -C out/Release skia
    INSTALL_COMMAND ""
)

# Link
# Link
add_library(skia_lib STATIC IMPORTED)
set_property(TARGET skia_lib PROPERTY IMPORTED_LOCATION "${BINARY_DIR}/out/Release/libskia.a")

# ALTERNATIVE: Use pre-built binaries (skia-binaries)
# If building from source is too heavy, fetch a release from a trusted provider.
# set(SKIA_PREBUILT_URL "https://github.com/skia-binaries/skia-binaries/releases/...")

```

**Minimal GN Args (CPU only):**
```bash
# Concrete GN Args for Static CPU-Only Linux Build
is_official_build=true
skia_use_system_expat=false
skia_use_system_libjpeg_turbo=false
skia_use_system_libpng=false
skia_use_system_libwebp=false
skia_use_system_zlib=false
skia_use_gl=false
skia_use_vulkan=false
skia_use_metal=false
skia_enable_pdf=false
skia_enable_skottie=false
skia_enable_gpu=false
skia_enable_fontmgr_empty=true
extra_cflags=["-DSK_FORCE_RASTER_PIPELINE_BLITTER"]
```

## 3. Initialization (CPU-Only)

Use `SkSurface::MakeRasterDirect` to wrap our benchmark buffer.

```cpp
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"

// 1. Info
// Skia prefers Premul. If our IR is straight alpha, we must account for it, 
// but standard vector rendering is usually premul compositing anyway.
SkImageInfo info = SkImageInfo::Make(
    width, height, 
    kBGRA_8888_SkColorType, // or kRGBA...
    kPremul_SkAlphaType
);

// 2. Surface
// buffer.data() must match the info.minRowBytes() usually (width * 4)
sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(
    info, 
    buffer.data(), 
    width * 4
);

// 3. Canvas
SkCanvas* canvas = surface->getCanvas();
```

## 4. Feature Mapping

| IR Feature | Skia API |
|---|---|
| **Path** | `SkPath` |
| **Move/Line/Curve** | `path.moveTo`, `path.cubicTo`, `path.close` |
| **Fill (Solid)** | `SkPaint p; p.setColor(...)` + `canvas->drawPath(path, p)` |
| **Fill (Gradient)** | `SkGradientShader::MakeLinear(...)` -> `p.setShader(...)` |
| **Fill Rule** | `path.setFillType(SkPathFillType::kWinding / kEvenOdd)` |
| **Stroke** | `p.setStyle(SkPaint::kStroke_Style); canvas->drawPath(...)` |
| **Stroke Config** | `p.setStrokeWidth`, `p.setStrokeCap`, `p.setStrokeJoin`, `SkDashPathEffect` |
| **Transforms** | `canvas->setMatrix(SkMatrix)` |

## 5. Notes

*   **Thread Safety**: `SkSurface` and `SkCanvas` are not thread safe. Use separate instances. `SkPath` and `SkPaint` are efficient copy-on-write value types.
*   **Globals**: `SkGraphics::Init()` is no longer strictly required in recent versions but good practice to check if font manager or other globals are needed (we ignore fonts).
