<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: PlutoVG

## Overview
PlutoVG is a standalone C vector graphics library. It is lightweight, has no dependencies, and is easy to embed.

## CPU-Only Configuration

### Surface Creation
PlutoVG supports creating a surface from an existing data buffer.

```c
#include <plutovg.h>

// 1. Create Surface
plutovg_surface_t* surface = plutovg_surface_create_for_data(
    buffer.data(),
    width,
    height,
    width * 4 // stride
);

// 2. Create Content
plutovg_t* pluto = plutovg_create(surface);

// 3. Draw
plutovg_set_source_rgb(pluto, 1, 0, 0);
plutovg_rect(pluto, 10, 10, 100, 100);
plutovg_fill(pluto);
```

### Build Instructions
PlutoVG is usually a single-header or few-source-files library.
*   **Integration**: Add `plutovg/source` to `include_directories` and sources to `add_library`.
*   **Defines**: None specific required for basic usage.

## Mapping Concepts

| Benchmark IR | PlutoVG C API |
|---|---|
| `Path` | `plutovg_move_to`, `plutovg_line_to`... |
| `Paint` | `plutovg_set_source_rgba`, `plutovg_set_source_linear_gradient` |
| `Matrix` | `plutovg_matrix_t`, `plutovg_set_matrix` |
| `Fill (NonZero)` | `plutovg_fill` (Default is NonZero) |
| `Fill (EvenOdd)` | `plutovg_set_fill_rule(pluto, PLUTOVG_FILL_RULE_EVEN_ODD); plutovg_fill(...)` |

## Notes
*   **Stroking**: PlutoVG supports standard caps/joins/miter.
*   **Memory**: Ensure the lifecycle of the data buffer exceeds the lifecycle of the `plutovg_surface_t`.
