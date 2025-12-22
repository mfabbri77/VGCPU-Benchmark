<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Vello (CPU)

## Overview
Vello is a research graphics engine by the Linebender community, focusing on compute-centered rendering. While primary targets are GPU (wgpu), it has a CPU fallback (`vello_cpu` or fine-grained software rasterization).

## Integration Strategy: Rust Bridge

Vello is written in **Rust** and does not expose a stable C ABI by default. To integrate it into this C++ benchmark suite:

1.  **Create a Rust Static Library (`vello_bridge`)**:
    *   Expose `extern "C"` functions for initialization, surface creation, and rendering.
    *   Use `cbindgen` to generate the C header.

```rust
// vello_bridge/src/lib.rs
#[no_mangle]
pub extern "C" fn vello_bridge_render(
    width: u32, height: u32, 
    buffer: *mut u8, 
    commands: *const c_void
) -> u32 {
    // 1. Wrap buffer in a bitmap/image target
    // 2. Parse commands (or pass IR blob pointer)
    // 3. Render using vello::Renderer or vello_cpu
}
```

2.  **Linkage**:
    *   Compile the rust crate as `staticlib`.
    *   Link `libvello_bridge.a` in the CMake project.
    *   Ensure Rust toolchain is available in the build environment.

## CPU Configuration
*   Explicitly select the CPU pipeline. Vello may try to select a GPU adapter by default; the bridge MUST instantiate a software renderer (e.g., using `skia-safe` software fallback if vello wraps it, or vello's own fine-grained rasterizer if ready).
*   **Note**: As of late 2023, Vello's CPU backend is extremely experimental. If a pure Vello CPU path is unstable, this backend might wrap `vello_cpu` specifically.

## Mapping Concepts

| Benchmark IR | Rust Bridge API | Vello (Rust) |
|---|---|---|
| `Path` | `vello_path_create` | `kurbo::BezPath` |
| `Paint` | `vello_set_brush` | `peniko::Brush` |
| `Fill` | `vello_fill_path` | `Scene::fill` |
| `Stroke` | `vello_stroke_path` | `Scene::stroke` |

## Notes
*   **Performance**: The FFI boundary overhead should be negligible compared to rendering time.
*   **Dependency**: Requires `cargo` and `rustc`.
