<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Anti-Grain Geometry (AGG)

## 1. Version and Dependencies

*   **Version**: 2.6
*   **Source Repository**: `https://github.com/ghaerr/agg-2.6`
*   **Commit Hash**: `e8c0757` (or latest stable on master)
*   **License**: Modified BSD (3-clause) / AGG License (permissive)

## 2. CMake Integration

AGG is a source-only library. Usage via `FetchContent` is recommended.

```cmake
include(FetchContent)

FetchContent_Declare(
    agg
    GIT_REPOSITORY https://github.com/ghaerr/agg-2.6.git
    GIT_TAG        master # Pin to specific commit in production
)
FetchContent_MakeAvailable(agg)

# Define an interface target if not provided by the repo
add_library(agg_interface INTERFACE)
target_include_directories(agg_interface INTERFACE 
    ${agg_SOURCE_DIR}/include 
    ${agg_SOURCE_DIR}/font_freetype
)
# Source files must be added if the repo doesn't expose a clean target
# Typically we need src/*.cpp
# NOTE: Verify the `agg` repo structure. Some forks put sources in `src/` and others in root.
file(GLOB AGG_SOURCES ${agg_SOURCE_DIR}/src/*.cpp)
add_library(agg_lib STATIC ${AGG_SOURCES})
target_link_libraries(agg_lib PUBLIC agg_interface)
```

## 3. Initialization (CPU-Only)

AGG requires explicit pipeline construction.

```cpp
#include "agg_rendering_buffer.h"
#include "agg_renderer_base.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_p.h"
#include "agg_renderer_scanline.h"
#include "agg_path_storage.h"
#include "agg_conv_stroke.h"

// 1. Wrap Buffer
// Buffer must be pre-allocated (Width * Height * 4 bytes)
agg::rendering_buffer rbuf(buffer.data(), width, height, stride);

// 2. Pixel Format (ARGB8888 Premultiplied)
using pixfmt_t = agg::pixfmt_rgba32_pre; 
pixfmt_t pixf(rbuf);

// 3. Base Renderer
using ren_base_t = agg::renderer_base<pixfmt_t>;
ren_base_t ren_base(pixf);

// 4. Rasterizer & Scanline
agg::rasterizer_scanline_aa<> ras;
agg::scanline_p8 sl;

// 5. Clear
ren_base.clear(agg::rgba(1, 1, 1, 0)); // Transparent
```

## 4. Feature Mapping

| IR Feature | AGG Implementation |
|---|---|
| **Path** | `agg::path_storage` |
| **Move/Line/Curve** | `path.move_to(x,y)`, `path.line_to(x,y)`, `path.curve4(x,y...)` |
| **Fill (Solid)** | `agg::render_scanlines_aa_solid(ras, sl, ren_base, color)` |
| **Fill (Linear)** | `agg::span_gradient` + `agg::span_allocator` + `agg::render_scanlines_aa` |
| **Fill Rule** | `ras.filling_rule(agg::fill_non_zero)` or `agg::fill_even_odd` |
| **Stroke** | Pipeline: `Path` -> `agg::conv_stroke` -> `ras.add_path` |
| **Stroke Caps** | `stroke.line_cap(agg::butt_cap / round_cap / square_cap)` |
| **Stroke Joins** | `stroke.line_join(agg::miter_join / round_join / bevel_join)` |
| **Transforms** | `agg::trans_affine` applied to path or pipeline stage |

## 5. Notes

*   **Pipeline**: AGG is a pipeline architecture. To apply a transform, use `agg::conv_transform` or transform points before adding to `path_storage`.
*   **Precision**: AGG uses software floating point rasterization (high quality).
*   **Thread Safety**: AGG objects are generally not thread-safe; use one instance per thread.
