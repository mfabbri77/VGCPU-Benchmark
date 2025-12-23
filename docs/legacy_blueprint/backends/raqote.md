<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Raqote

> **CPU-Only**: Raqote is a pure Rust CPU software renderer designed for speed and simplicity. No GPU variant exists.

## 1. Version and Dependencies

*   **Version**: 0.8.5
*   **Source**: Crates.io / GitHub `https://github.com/jrmuizel/raqote`
*   **Dependencies**: Rust toolchain (stable)
*   **License**: Apache 2.0 / MIT
*   **Rendering**: Pure CPU software rasterization with SIMD and multi-threading optimization (via Corrosion)

## 2. CMake Integration (via Corrosion)

We use `Corrosion` to seamlessy import the Rust crate into CMake.

```cmake
include(FetchContent)

# 1. Fetch Corrosion (Rust-CMake Bridge)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG        v0.4.7 # Pin version
)
FetchContent_MakeAvailable(Corrosion)

# 2. Import Rust Crate
# Assumes generic wrapper crate "raqote_bridge" exists in: backends/raqote/rust_bridge/
corrosion_import_crate(MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rust_bridge/Cargo.toml")

# 3. Link
add_library(raqote_adapter ...)
target_link_libraries(raqote_adapter PRIVATE raqote_bridge)
```

## 3. Initialization (CPU-Only)

Raqote is CPU-only by definition.

**Rust Bridge (`rust_bridge/src/lib.rs`):**

```rust
use raqote::{DrawTarget, SolidSource, Source, DrawOptions, PathBuilder, Transform};
use std::slice;

#[repr(C)]
pub struct RqtSurface {
    dt: DrawTarget,
}

#[no_mangle]
pub extern "C" fn rqt_create(width: i32, height: i32) -> *mut RqtSurface {
    let dt = DrawTarget::new(width, height);
    Box::into_raw(Box::new(RqtSurface { dt }))
}

#[no_mangle]
pub extern "C" fn rqt_destroy(ptr: *mut RqtSurface) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn rqt_clear(ptr: *mut RqtSurface, r: u8, g: u8, b: u8, a: u8) {
    let surf = unsafe { &mut *ptr };
    let color = SolidSource::from_rgba8(r, g, b, a);
    surf.dt.clear(color);
}

#[no_mangle]
pub extern "C" fn rqt_get_pixels(ptr: *mut RqtSurface, out_buf: *mut u32) {
    let surf = unsafe { &mut *ptr };
    let data = surf.dt.get_data();
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), out_buf, data.len());
    }
}

// Example primitive (expand for full IR mapping)
#[no_mangle]
pub extern "C" fn rqt_fill_rect(ptr: *mut RqtSurface, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8, a: u8) {
    let surf = unsafe { &mut *ptr };
    let mut pb = PathBuilder::new();
    pb.rect(x, y, w, h);
    let path = pb.finish();
    let src = Source::Solid(SolidSource::from_rgba8(r, g, b, a));
    surf.dt.fill(&path, &src, &DrawOptions::default());
}
```

**C++ Side:**
```cpp
struct RqtSurface;
extern "C" {
    RqtSurface* rqt_create(int w, int h);
    void rqt_destroy(RqtSurface* s);
    void rqt_clear(RqtSurface* s, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void rqt_get_pixels(RqtSurface* s, uint32_t* buf);
    void rqt_fill_rect(RqtSurface* s, float x, float y, float w, float h, ...);
}

// Adapter Render() implementation:
auto* surf = rqt_create(width, height);
// ... replay IR commands calling rqt_xxxx ...
rqt_get_pixels(surf, (uint32_t*)buffer.data());
rqt_destroy(surf);
```

## 4. Feature Mapping

| IR Feature | Raqote API |
|---|---|
| **Path** | `raqote::Path` (Command iterator) |
| **Move/Line/Curve** | `PathBuilder::new().move_to()...` |
| **Fill (Solid)** | `dt.fill(path, &Source::Solid(Color), &DrawOptions)` |
| **Fill (Gradient)** | `Source::new_linear_gradient(...)` |
| **Fill Rule** | `DrawOptions { fill_rule: FillRule::NonZero / EvenOdd, ... }` |
| **Stroke** | `dt.stroke(path, &Source, &StrokeStyle, &DrawOptions)` |
| **Stroke Config** | `StrokeStyle { width, cap, join, dash_array, ... }` |
| **Transforms** | `Transform::create_matrix(...)`. Apply to `dt.set_transform()`. |

## 5. Notes

*   **Bridge Overhead**: Since Raqote is Rust-only, the adapter must either serialize IR to the bridge or expose a fine-grained C API from Rust. We recommend a fine-grained C API `rqt_move_to`, `rqt_fill`, etc., to avoid re-parsing IR.
*   **FPU**: Raqote uses `f32`.
