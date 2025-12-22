# Backend Integration: Anti-Grain Geometry (AGG)

## Overview
AGG is a high-quality, template-based C++ rendering library. It is famous for subpixel accuracy. We target **AGG 2.4** (or 2.6), the BSD-licensed version.

## CPU-Only Configuration

AGG is strictly a CPU rasterizer.

### Pipeline Setup
AGG requires composing a rendering pipeline via templates.

```cpp
#include "agg_rendering_buffer.h"
#include "agg_renderer_base.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_p.h"
#include "agg_renderer_scanline.h"

// 1. Buffer Wrapper
agg::rendering_buffer rbuf(buffer.data(), width, height, stride);

// 2. Pixel Format
using pixfmt_t = agg::pixfmt_rgba32_pre; // ARGB8888 premul
pixfmt_t pixf(rbuf);

// 3. Base Renderer
using ren_base_t = agg::renderer_base<pixfmt_t>;
ren_base_t ren_base(pixf);

// 4. Rasterizer & Scanline
agg::rasterizer_scanline_aa<> ras;
agg::scanline_p8 sl;

// 5. Render
ras.add_path(path);
agg::render_scanlines_aa_solid(ras, sl, ren_base, agg::rgba(1,0,0,1));
```

## Build Instructions
*   **Source**: AGG is often distributed as source.
*   **Defines**: None specific.
*   **Standard**: C++98 compliant (very portable).

## Mapping Concepts

| Benchmark IR | AGG API |
|---|---|
| `Path` | `agg::path_storage` |
| `Paint` | `agg::rgba` (Color), `span_gradient` (Gradient) |
| `Matrix` | `agg::trans_affine` |
| `Fill` | `agg::render_scanlines_aa_solid` or `_bin` |
| `Stroke` | `agg::conv_stroke<agg::path_storage>` (Pipeline stage) |

## Notes
*   **Verbosity**: AGG requires explicit pipeline construction (e.g., stroking is a converter `conv_stroke` applied to a path source before rasterization). Adapter must manage these pipelines efficiently.
