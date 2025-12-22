<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Blend2D

## 1. Version and Dependencies

*   **Version**: 0.21.2 (Latest Stable)
*   **Repository**: `https://github.com/blend2d/blend2d`
*   **Dependencies**: `asmjit` (automatically handled by Blend2D's CMake if nested)
*   **License**: Zlib

## 2. CMake Integration

Blend2D supports CMake natively.

```cmake
include(FetchContent)

# 1. AsmJit (Required Dependency)
FetchContent_Declare(
    asmjit
    GIT_REPOSITORY https://github.com/asmjit/asmjit.git
    GIT_TAG        master # Or aligned version
)
FetchContent_MakeAvailable(asmjit)

# 2. Blend2D
FetchContent_Declare(
    blend2d
    GIT_REPOSITORY https://github.com/blend2d/blend2d.git
    GIT_TAG        v0.21.2 # Pin to stable release
)
set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
set(BLEND2D_TEST OFF CACHE BOOL "" FORCE)
set(BLEND2D_NO_JIT OFF CACHE BOOL "" FORCE) # JIT is CPU-only, permitted
FetchContent_MakeAvailable(blend2d)

# Link
target_link_libraries(my_adapter PRIVATE blend2d::blend2d)
```

## 3. Initialization (CPU-Only)

Blend2D is primarily a CPU rasterizer. `BLImage` provides the pixel buffer.

```cpp
#include <blend2d.h>

// 1. Setup Buffer
// We can wrap existing memory or let BLImage allocate.
BLImage img;
img.createFromData(width, height, BL_FORMAT_PRGB32, buffer.data(), stride);

// 2. Attach Context
BLContext ctx(img);

// 3. Threading Policy
// Set to 0 to force single-threaded for strict comparison, 
// or N to benchmark multi-core scaling.
ctx.setThreadCount(1); 
```

## 4. Feature Mapping

| IR Feature | Blend2D API |
|---|---|
| **Path** | `BLPath` |
| **Move/Line/Curve** | `path.moveTo`, `path.lineTo`, `path.cubicTo` |
| **Fill (Solid)** | `ctx.setFillStyle(BLRgba32(r,g,b,a))` |
| **Fill (Gradient)** | `BLGradient` (Linear/Radial types) -> `ctx.setFillStyle(gradient)` |
| **Fill Rule** | `ctx.setFillRule(BL_FILL_RULE_NON_ZERO / EVEN_ODD)` |
| **Stroke** | `ctx.strokePath(path)` |
| **Stroke Config** | `ctx.setStrokeWidth`, `ctx.setStrokeCaps`, `ctx.setStrokeJoin` |
| **Transforms** | `ctx.setMatrix`, `ctx.transform` |

## 5. Notes

*   **JIT**: Blend2D's JIT compiles rasterization pipelines. This is valid "CPU" work.
*   **Precision**: High quality by default.
*   **Async**: Ensure all work is synchronous (default behavior) or flush if using threaded rendering.
