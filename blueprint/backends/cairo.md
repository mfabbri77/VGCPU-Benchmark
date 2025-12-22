<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Cairo (Image Surface)

> **CPU-Only**: This backend uses `cairo_image_surface_create` for pure CPU software rasterization via the Pixman library. For optimal performance, ensure Pixman is compiled with SIMD support (SSE2/AVX/NEON).

## 1. Version and Dependencies

*   **Version**: 1.18.2 (Stable 2024)
*   **Repository**: `https://gitlab.freedesktop.org/cairo/cairo.git`
*   **Dependencies**: Pixman (Required, > 0.40.0), Freetype/Fontconfig (often pulled in but unused for us), Zlib, PNG.
*   **License**: LGPL-2.1 / MPL-1.1
*   **Rendering**: Pure CPU software rasterization via Pixman

## 2. CMake Integration

Cairo is historically built with Autotools/Meson, but CMake support exists in forks or via explicit commands. For this benchmark, we recommend using system libraries if available (consistent on Linux) or a `FetchContent` wrapper for Meson. However, `vcpkg` or `conan` is often busiest for Cairo. 

**Recommended Approach: PkgConfig / FindPackage on Linux, Vcpkg on Windows/Mac.**

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo>=1.16.0)

if (CAIRO_FOUND)
    add_library(cairo_adapter INTERFACE)
    target_include_directories(cairo_adapter INTERFACE ${CAIRO_INCLUDE_DIRS})
    target_link_libraries(cairo_adapter INTERFACE ${CAIRO_LIBRARIES})
else()
    # Windows Alternative: Manual Precompiled Binaries
    # Source: https://github.com/preshing/cairo-windows
    set(LOCAL_CAIRO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/cairo/cairo-windows-1.17.2")
    if(EXISTS "${LOCAL_CAIRO_ROOT}/include/cairo.h")
        add_library(cairo::cairo SHARED IMPORTED)
        set_target_properties(cairo::cairo PROPERTIES
            IMPORTED_IMPLIB "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.lib"
            IMPORTED_LOCATION "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${LOCAL_CAIRO_ROOT}/include"
        )
        add_library(PkgConfig::CAIRO ALIAS cairo::cairo)
        set(CAIRO_FOUND TRUE)
    endif()

    if(NOT CAIRO_FOUND)
        message(FATAL_ERROR "Please install libcairo2-dev (Linux), use vcpkg (Windows/Mac), or place precompiled binaries in deps/cairo.")
    endif()
endif()
```

**Windows Alternative (Precompiled)**
On Windows, if vcpkg fails (e.g., due to dependency build issues), you can use precompiled binaries from `preshing/cairo-windows`.
1. Download `cairo-windows-1.17.2.zip`.
2. Extract to `deps/cairo`.
3. CMake will automatically detect it.
```

## 3. Initialization (CPU-Only)

Cairo Image Surface (`CAIRO_FORMAT_ARGB32`) is the standard software rasterizer.

```cpp
#include <cairo.h>

// 1. Wrap Buffer
// Stride alignment is critical. Cairo requires specific stride aligment (usually 4 bytes, but safe to ask).
int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
// NOTE: If our harness buffer stride != cairo stride, we must account for it or copy.
// The benchmark should prefer allocating with cairo stride or copy-free compatible padding.

cairo_surface_t* surface = cairo_image_surface_create_for_data(
    buffer.data(),
    CAIRO_FORMAT_ARGB32,
    width,
    height,
    stride
);

// 2. Create Context
cairo_t* cr = cairo_create(surface);

// 3. Defaults
cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
```

## 4. Feature Mapping

| IR Feature | Cairo API |
|---|---|
| **Path** | `cairo_new_path`, `cairo_move_to`, `cairo_line_to`... |
| **Fill (Solid)** | `cairo_set_source_rgba`; `cairo_fill` |
| **Fill (Gradient)** | `cairo_pattern_create_linear/radial` -> `cairo_set_source` |
| **Fill Rule** | `cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING / EVEN_ODD)` |
| **Stroke** | `cairo_stroke(cr)` |
| **Stroke Config** | `cairo_set_line_width`, `cairo_set_line_cap`, `cairo_set_line_join`, `cairo_set_dash` |
| **Transforms** | `cairo_matrix_init`, `cairo_transform` |

## 5. Notes

*   **Stateful**: Cairo is a state machine. The adapter must sync generic IR state (paint, transform) into the Cairo context before every operation.
*   **Pixman**: Ensure Pixman is compiled with SIMD (SSE2/AVX/NEON) for representative performance.
