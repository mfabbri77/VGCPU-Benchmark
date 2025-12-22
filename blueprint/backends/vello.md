<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Vello (CPU)

## 1. Version and Dependencies

*   **Version**: Experimental (Pin to latest `linebender/vello` commit)
*   **Repository**: `https://github.com/linebender/vello`
*   **Dependencies**: Rust toolchain, `wgpu` (even for CPU, sometimes needed for types, though goal is strict CPU).
*   **License**: Apache 2.0 / MIT

## 2. CMake Integration (via Corrosion)

Vello is pure Rust. Integration requires Corrosion and helper crate.

```cmake
include(FetchContent)
FetchContent_Declare(Corrosion ...) # See Raqote
corrosion_import_crate(MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vello_bridge/Cargo.toml")
```

## 3. Initialization (CPU-Only)

Vello is GPU-first. CPU support might be via `skia-safe` fallback or `vello-encoding` software rasterization.
**Verification Required**: If Vello's own software rasterizer is not ready, this backend might essentially be a wrapper around Skia-via-Rust.
For this benchmark, we target **Vello's Fine-Grained Rasterizer** (if functional) or label it "Experimental".

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

*   **Experimental Status**: This backend is considered high-risk. If a pure CPU path is not available or too slow (just an interpreter), it should be marked as `experimental` in the benchmark.
*   **Bridge**: Reuse the pattern from Raqote (C FFI + Corrosion).
