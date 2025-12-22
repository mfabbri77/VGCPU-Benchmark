<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: ThorVG SW

> **CPU-Only**: This backend uses ThorVG's SW (Software) engine for pure CPU/SIMD rasterization. GPU backends (GL, WGL, WGPU) are NOT in scope for this benchmark.

## 1. Version and Dependencies

*   **Version**: v1.0-pre34 (Latest Release Dec 2025)
*   **Repository**: `https://github.com/thorvg/thorvg`
*   **Dependencies**: None (for SW engine).
*   **License**: MIT
*   **Rendering**: Pure CPU software rasterization with SIMD optimization

## 2. CMake Integration

ThorVG uses CMake and supports FetchContent.

```cmake
include(FetchContent)

FetchContent_Declare(
    thorvg
    GIT_REPOSITORY https://github.com/thorvg/thorvg.git
    GIT_TAG        v0.15.16 # Stable Release (Oct 2025)
)

# Configuration options to minimize build
set(TVG_ENGINES "SW" CACHE STRING "" FORCE)
set(TVG_LOADERS "" CACHE STRING "" FORCE) # Disable SVG/Lottie loaders if we use C++ API directly
set(TVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(thorvg)

target_link_libraries(my_adapter PRIVATE thorvg)
```

## 3. Initialization (CPU-Only)

ThorVG SW engine writes directly to memory.

```cpp
#include <thorvg.h>

// 1. Engine Init
// Call once per process or handle via Reference Checking
if (tvg::Initializer::init(tvg::CanvasEngine::Sw, 1) != tvg::Result::Success) {
    // Error
}

// 2. Wrap Buffer
auto canvas = tvg::SwCanvas::gen();
// target(buffer, stride, w, h, colorspace)
canvas->target((uint32_t*)buffer.data(), width, width, height, tvg::SwCanvas::ARGB8888);

// 3. Clear
canvas->clear(0, 0, 0, 0); // Optional if new buffer
```

## 4. Feature Mapping

| IR Feature | ThorVG API |
|---|---|
| **Path** | `tvg::Shape` (acts as both path and paint container) |
| **Move/Line/Curve** | `shape->moveTo`, `shape->lineTo`, `shape->cubicTo` |
| **Fill (Solid)** | `shape->fill(r, g, b, a)` |
| **Fill (Gradient)** | `tvg::LinearGradient` -> `shape->fill(grad)` |
| **Fill Rule** | `shape->fill(tvg::FillRule::NonZero / EvenOdd)` |
| **Stroke** | `shape->strokeWidth(...)`, `shape->strokeFill(...)` |
| **Stroke Config** | `shape->strokeCap(...)`, `shape->strokeJoin(...)`, `shape->strokeDash(...)` |
| **Transforms** | `shape->transform(matrix)` |

## 5. Notes

*   **Sync**: Drawing commands are queued. `canvas->sync()` is the rasterization trigger. **Measurement must include sync()**.
*   **Object Semantics**: In ThorVG, a `Shape` bundles geometry and styling. This differs from Skia/Cairo where Paint is separate. The adapter might need to manage pools of `Shape` objects or reset them frequently.
