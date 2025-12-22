# Backend Integration: ThorVG

## Overview
ThorVG is a platform-independent, portable library for vector graphics scenes and basic animation (Lottie). It is optimized for small binary size and high performance on embedded systems.

## CPU-Only Configuration

### Surface Creation
ThorVG provides a specific canvas for software (CPU) rasterization: `tvg::SwCanvas`.

```cpp
#include <thorvg.h>

// 1. Initialize Engine
tvg::Initializer::init(tvg::CanvasEngine::Sw, 0);

// 2. Create Canvas
auto canvas = tvg::SwCanvas::gen();

// 3. Set Target Buffer
// ThorVG writes directly to this buffer. format is typically ARGB8888.
canvas->target((uint32_t*)buffer.data(), width, width, height, tvg::SwCanvas::ARGB8888);

// 4. Render
canvas->push(shape);
canvas->draw();
canvas->sync(); // Rasterization happens here
```

### Build Instructions
ThorVG provides a CMake build system.
*   **Static Linking**: `DISABLE_SHARED_LIBS=ON` (or standard BUILD_SHARED_LIBS=OFF)
*   **Loaders**: Disable loaders we don't need (SVG, Lottie are core, but JPG/PNG might be optional) to minimize deps.
*   **Engines**: Enable only `SW` engine.

```cmake
# CMake Options
set(TVG_ENGINES "SW")
set(TVG_BUILD_EXAMPLES OFF)
```

## Mapping Concepts

| Benchmark IR | ThorVG C++ API |
|---|---|
| `Path` | `tvg::Shape` (appendPath, moveTo, lineTo...) |
| `Paint` | `tvg::Shape->fill(r, g, b, a)` or `tvg::LinearGradient` |
| `Matrix` | `tvg::Shape->transform(...)` |
| `FillPath` | `canvas->push(shape)` (shape has Default fill) |
| `Stroke` | `tvg::Shape->strokeWidth(...)`, `tvg::Shape->strokeColor(...)` |

## Notes
*   **Coordinates**: ThorVG uses float coordinates.
*   **Sync**: `canvas->draw()` queues commands; `canvas->sync()` executes them. Timing measurements MUST encompass the `sync()` call.
