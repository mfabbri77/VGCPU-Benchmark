<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Raqote

## 1. Version and Dependencies

*   **Version**: 0.8.5
*   **Source**: Crates.io / GitHub `https://github.com/jrmuizel/raqote`
*   **Dependencies**: Rust toolchain (cargo, rustc), `font-kit` (optional, can be disabled).
*   **License**: MIT / Apache 2.0

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

**Rust Side (`lib.rs`):**
```rust
#[no_mangle]
pub extern "C" fn raqote_render(
    width: i32, height: i32, 
    buffer: *mut u32
) {
    let mut dt = DrawTarget::new(width, height);
    // ... replay commands ...
    // Copy out
    let data = dt.get_data(); // slice of u32
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), buffer, data.len());
    }
}
```

**C++ Side:**
```cpp
extern "C" void raqote_render(int w, int h, uint32_t* buf);

// Adapter Render() implementation:
raqote_render(width, height, (uint32_t*)buffer.data());
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
