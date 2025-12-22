<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Cairo

## Overview
Cairo is a 2D graphics library with support for multiple output devices. It is the longest-standing open-source vector graphics library, used by GTK and Firefox.

## CPU-Only Configuration

### Surface Creation
Use the **Image Surface** backend (`cairo_image_surface`) which renders to a memory buffer on the CPU.

```cpp
#include <cairo/cairo.h>

// 1. Create Surface from Data
int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
cairo_surface_t* surface = cairo_image_surface_create_for_data(
    buffer.data(),
    CAIRO_FORMAT_ARGB32,
    width,
    height,
    stride
);

// 2. Create Context
cairo_t* cr = cairo_create(surface);

// 3. Render
cairo_set_source_rgba(cr, 1, 0, 0, 1);
cairo_rectangle(cr, 10, 10, 100, 100);
cairo_fill(cr);
```

### Build Instructions
Cairo has a complex dependency tree (pixman, png, zlib, fontconfig, freetype).
*   **Minimization**: For this benchmark, we can disable PDF, SVG, PS, X11, Win32, Quartz surfaces if dynamic dispatch allows, but `cairo` usually builds them all.
*   **Pixman**: Is the low-level pixel manipulation library used by Cairo. It MUST be compiled with SIMD optimizations (SSE2/NEON) for fair performance.

## Mapping Concepts

| Benchmark IR | Cairo C API |
|---|---|
| `Path` | `cairo_new_path`, `cairo_move_to`, `cairo_line_to`... |
| `Paint` | `cairo_set_source_rgba` or `cairo_pattern_create_linear` |
| `Matrix` | `cairo_matrix_t`, `cairo_transform` |
| `FillPath` | `cairo_fill` (or `cairo_fill_preserve`) |
| `StrokePath` | `cairo_stroke` |
| `Save`/`Restore` | `cairo_save` / `cairo_restore` |

## Notes
*   **Winding Rules**: Cairo supports NonZero (`CAIRO_FILL_RULE_WINDING`) and EvenOdd.
*   **State**: Cairo paints are part of the context state (`cairo_set_source`), whereas in our IR paints might be separate objects. The adapter must invoke `cairo_set_source` before every draw if the paint changes.
