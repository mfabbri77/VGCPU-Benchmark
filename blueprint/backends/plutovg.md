<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: PlutoVG

## 1. Version and Dependencies

*   **Version**: v1.3.2 (Dec 2025)
*   **Repository**: `https://github.com/sammycage/plutovg`
*   **Dependencies**: None (Standalone)
*   **License**: MIT

## 2. CMake Integration

PlutoVG is designed to be trivially embedded.

```cmake
include(FetchContent)

FetchContent_Declare(
    plutovg
    GIT_REPOSITORY https://github.com/sammycage/plutovg.git
    GIT_TAG        v1.3.2
)
FetchContent_MakeAvailable(plutovg)

# Link
target_link_libraries(my_adapter PRIVATE plutovg)
```

## 3. Initialization (CPU-Only)

PlutoVG rasterizes to a memory buffer.

```cpp
#include <plutovg.h>

// 1. Create Surface
plutovg_surface_t* surface = plutovg_surface_create_for_data(
    buffer.data(),
    width,
    height,
    width * 4 // Stride (assuming packed ARGB)
);

// 2. Create Context
plutovg_t* pluto = plutovg_create(surface);

// 3. Clear
// Use operator Clear or draw a rectangle.
```

## 4. Feature Mapping

| IR Feature | PlutoVG API |
|---|---|
| **Path** | `plutovg_move_to`, `plutovg_line_to`... |
| **Fill (Solid)** | `plutovg_set_source_rgba`; `plutovg_fill` |
| **Fill (Gradient)** | `plutovg_set_source_linear_gradient`... |
| **Fill Rule** | `plutovg_set_fill_rule(pluto, PLUTOVG_FILL_RULE_NON_ZERO / EVEN_ODD)` |
| **Stroke** | `plutovg_stroke` |
| **Stroke Config** | `plutovg_set_line_width`, `plutovg_set_line_cap`, `plutovg_set_line_join` |
| **Transforms** | `plutovg_matrix_init`, `plutovg_set_matrix` |

## 5. Notes

*   **Simplicity**: PlutoVG is very concise.
*   **Thread Safety**: Contexts are not thread-safe.
