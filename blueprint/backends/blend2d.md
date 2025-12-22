<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Blend2D

## Overview
Blend2D is a high-performance 2D vector graphics engine written in C++. It features a JIT compiler to generate optimized rasterization pipelines at runtime.

## CPU-Only Configuration

### Surface Creation
Blend2D's main image class `BLImage` is inherently a CPU raster buffer.

```cpp
// 1. Create Image
BLImage img(width, height, BL_FORMAT_PRGB32); // Premultiplied RGB 32-bit

// 2. Attached Context
BLContext ctx(img);

// 3. Render
ctx.setFillStyle(BLRgba32(0xFF0000FF));
ctx.fillCircle(100, 100, 50);

// 4. Access Pixel Data
BLImageData data;
img.getData(&data); 
// data.pixelData points to raw bytes
// data.stride is row stride
```

### Build Instructions
Blend2D uses CMake natively, making it trivial to integrate via `FetchContent` or `git submodule`.

```cmake
# CMakeList.txt
set(BLEND2D_STATIC ON)
set(BLEND2D_NO_JIT OFF) # Keep JIT ON for performance
add_subdirectory(blend2d)
target_link_libraries(my_bench blend2d::blend2d)
```

## Mapping Concepts

| Benchmark IR | Blend2D C++ API |
|---|---|
| `Path` | `BLPath` |
| `Paint` | `BLVar` (can hold Color, Gradient) or `ctx.setFillStyle` directly |
| `Matrix` | `BLMatrix2D` |
| `Clear` | `ctx.clearAll()` |
| `Save` / `Restore` | `ctx.save()` / `ctx.restore()` |

## Notes
*   **Threading**: Blend2D can use multiple threads for rendering. For fair "single-threaded" comparisons, we should configure the thread pool to 0 or 1 thread if exposed, though defaults are usually fine for simple scenes.
*   **JIT**: The JIT is a CPU feature, not GPU. It should remain enabled as it is the core differentiator of Blend2D's architecture.
