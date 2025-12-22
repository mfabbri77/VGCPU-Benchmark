# Backend Integration: Raqote

## Overview
Raqote is a pure Rust 2D vector graphics library, often described as a backend-independent Skia alternative. It is CPU-only.

## Integration Strategy: Rust Bridge

Like Vello, Raqote is Rust-only and has no off-the-shelf C API. We must wrap it.

1.  **Wrapper Crate**: Create `raqote_bridge`.
2.  **C API**:
    ```c
    void* raqote_create_dt(int width, int height);
    void raqote_draw_path(void* dt, float* path_cmds, size_t count);
    void raqote_get_pixels(void* dt, uint8_t* out_buf);
    ```
3.  **Implementation**:
    *   Use `raqote::DrawTarget`.
    *   `DrawTarget::new(width, height)` creates the surface.
    *   `get_data_u8()` returns pixel slice.

## Build Instructions
*   Add a CMake rule to build the cargo project as `staticlib`.
*   Link `libraqote_bridge.a`.

## Mapping Concepts

| Benchmark IR | Raqote (Rust) |
|---|---|
| `Path` | `raqote::Path` (similar verbs to Skia) |
| `Paint` | `raqote::Source` (Solid, Linear, Radial) |
| `Fill` | `dt.fill(...)` |
| `Stroke` | `dt.stroke(...)` |
| `BlendMode` | `raqote::BlendMode::SrcOver` |

## Notes
*   **Dependencies**: `font-kit` is optional if we don't do text (we don't).
*   **Performance**: Raqote is single-threaded.
