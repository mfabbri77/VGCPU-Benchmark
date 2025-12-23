<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: vello_cpu

> **CPU-Only**: This backend uses the `vello_cpu` crate, the pure CPU fallback for Vello. The main Vello engine (GPU compute shaders) is NOT in scope for this benchmark. Note: `vello_cpu` is currently in **alpha** status.

## 1. Version and Dependencies

*   **Version**: Experimental (Pin to latest `linebender/vello` commit)
*   **Repository**: `https://github.com/linebender/vello`
*   **Crate**: `vello_cpu` (https://crates.io/crates/vello_cpu)
*   **Dependencies**: Rust toolchain (stable)
*   **License**: Apache 2.0 / MIT
*   **Rendering**: Pure CPU software rasterization with SIMD and multi-threading optimization

## 2. CMake Integration (via Corrosion)

Vello is pure Rust. Integration requires Corrosion and helper crate.

```cmake
include(FetchContent)
FetchContent_Declare(Corrosion ...) # See Raqote
corrosion_import_crate(MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vello_bridge/Cargo.toml")
```

## 3. Initialization (CPU-Only)

The `vello_cpu` crate provides a pure CPU implementation optimized for SIMD and multithreaded execution, offering high portability without requiring GPU configuration.

**Rust Bridge (`vello_bridge/src/lib.rs`):**
```rust
use vello::peniko::Color;
use vello::{Scene, Renderer, RendererOptions, RenderParams};
use vello::util::{RenderContext, RenderSurface};
use wgpu::{Device, Queue, TextureFormat};

// Vello is complex to bridge via C because it relies on wgpu async structs.
// For this benchmark, we assume a synchronous wrapper that instantiates a 
// single-threaded wgpu::Instance backed by CPU (if available) or simply passes 
// the Display List to a Rust function that handles the full render pass.

#[no_mangle]
pub extern "C" fn vello_render_cpu(
    width: u32, height: u32,
    out_buf: *mut u32,
    // Pass serialized IR or minimal commands here?
    // Better: Helper functions to build Scene
) {
    // 1. Setup Vello Context (expensive, should be persistent in real app)
    // 2. Build Scene
    let mut scene = Scene::new();
    // ... populate scene ...
    
    // 3. Render
    // This requires setting up wgpu with a "Software" backend (e.g., Lavapipe or explicit CPU fallback).
    // As of 2025, Vello might have a direct software rasterizer.
    // If NOT: This backend effectively benchmarks wgpu-on-cpu.
}
```

## 4. Feature Mapping

| IR Feature | Vello/Kurbo (Rust) |
|---|---|
| **Path** | `kurbo::BezPath` |
| **Move/Line/Curve** | `path.move_to`, `path.curve_to` |
| **Fill (Solid)** | `Scene::fill(path, brush)` |
| **Fill (Gradient)** | `peniko::Gradient` |
| **Fill Rule** | `NonZero` (Default), `EvenOdd` |
| **Stroke** | `Scene::stroke(path, brush, stroke_style)` |
| **Transforms** | `Affine` applied to `Scene` or path |

## 5. Notes

*   **Alpha Status**: `vello_cpu` is in early development. May have incomplete features or performance issues.
*   **Multi-Threading**: The CPU implementation is optimized for SIMD and multithreaded execution.
*   **Performance**: Reported to be faster than Skia software and tiny-skia due to SIMD optimizations.
*   **Bridge**: Reuse the pattern from Raqote (C FFI + Corrosion).
