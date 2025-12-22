<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: AmanithVG SRE

## 1. Version and Dependencies

*   **Version**: SRE (Software Rendering Engine) from latest SDK
*   **Source Repository**: `https://github.com/Mazatech/amanithvg-sdk`
*   **Commit Hash**: Latest stable on master
*   **License**: Proprietary / Evaluation (Check license file in SDK)

## 2. CMake Integration

Since AmanithVG is distributed as an SDK with precompiled static libraries and headers, `FetchContent` can be used to pull the SDK, but linking must point to the correct architecture folder.

```cmake
include(FetchContent)

FetchContent_Declare(
    amanithvg
    GIT_REPOSITORY https://github.com/Mazatech/amanithvg-sdk.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(amanithvg)

# Logic to select correct lib based on CMAKE_SYSTEM_PROCESSOR (x86_64, arm64) 
# and CMAKE_SYSTEM_NAME (Linux, Windows, Darwin).
# The SDK typically provides precompiled static libs in `lib/platform/...`.
set(AVG_LIB_PATH "${amanithvg_SOURCE_DIR}/lib/${AVG_PLATFORM_DIR}") 

# Define wrapper target
add_library(amanithvg_lib STATIC IMPORTED)

set_target_properties(amanithvg_lib PROPERTIES
    IMPORTED_LOCATION "${AVG_LIB_PATH}/libAmanithVG.a"
    INTERFACE_INCLUDE_DIRECTORIES "${amanithvg_SOURCE_DIR}/include"
    # CRITICAL: Define SRE engine mode for headers
    INTERFACE_COMPILE_DEFINITIONS "AMANITHVG_SRE=1" 
)
```

## 3. Initialization (CPU-Only)

AmanithVG SRE allows rendering to client memory without a full EGL context if using the proprietary extensions, or via a simplified EGL emulation provided by the SDK.

```cpp
#include <VG/openvg.h>
#include <VG/vgu.h>

// AmanithVG specific: Initialize SRE without GL context
// (Pseudocode based on SDK examples for software rasterizers)

// 1. Initialize Surface Parameters
void* buffer_ptr = buffer.data();
VGint width = ...;
VGint height = ...;
// AmanithVG SRE typically supports VG_sRGBA_8888_PRE natively.
// Verify stride alignment (usually 4 bytes for 32-bit formats).
VGImageFormat format = VG_sRGBA_8888_PRE;

// 2. Create Context & Surface
// The SDK provides a helper `vgPrivSurfaceCreateMZT` or similar in `src/extensions`.
// If not available, use the provided `vg_lite` or SRE specific initialization header.
// Standard OpenVG 1.1 doesn't define "headless surface creation" without EGL.
// For the SRE backend, look for:
// vgPrivMakeCurrentMZT(void *surface_pixels, VGint surface_width, VGint surface_height, VGint surface_stride, VGImageFormat surface_format);
vgPrivMakeCurrentMZT(buffer_ptr, width, height, stride, format);

// 3. Set Defaults
vgSeti(VG_RENDERING_QUALITY, VG_RENDERING_QUALITY_BETTER);
vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
// Reset matrices to Identity
vgLoadIdentity();
```

## 4. Feature Mapping

| IR Feature | OpenVG API |
|---|---|
| **Path** | `VGPath path = vgCreatePath(...)` |
| **Move/Line/Curve** | `vguLine`, `vguRect`, or `vgAppendPathData` |
| **Fill (Solid)** | `VGPaint paint = vgCreatePaint()`; `vgSetColor(paint, rgba)` |
| **Fill (Gradient)** | `vgSetPaint(paint, VG_FILL_PATH)`; `vgSetParameterfv(paint, VG_PAINT_COLOR_RAMP_STOPS, ...)` |
| **Fill Rule** | `vgSeti(VG_FILL_RULE, VG_EVEN_ODD / VG_NON_ZERO)` |
| **Stroke** | `vgDrawPath(path, VG_STROKE_PATH)` |
| **Stroke Config** | `vgSetf(VG_STROKE_LINE_WIDTH, w)`; `vgSeti(VG_STROKE_CAP_STYLE, ...)` |
| **Transforms** | `vgLoadMatrix`, `vgMultMatrix` (OpenVG uses global matrix state) |

## 5. Notes

*   **Context Safety**: OpenVG uses global context state. Parallel rendering requires strictly separate contexts per thread.
*   **Allocations**: OpenVG handles (Paths, Paints) are opaque resources and MUST be destroyed (`vgDestroyPath`) to avoid leaks.
