# Backend Integration: AmanithVG SRE

## Overview
AmanithVG SRE is a proprietary (evaluated as free) implementation of the OpenVG 1.1 standard. It is a high-performance software rasterizer.

## CPU-Only Configuration

AmanithVG SRE is intrinsically CPU-only.

### Surface Initialization
OpenVG relies on EGL for surface creation, but AmanithVG provides a custom EGL implementation or a direct interface.

```cpp
#include <VG/openvg.h>
#include <VG/vgu.h>
// AmanithVG specific header for surface creation might be needed, 
// usually it mimics EGL or provides "vgPrivSurfaceCreateMZT".

// Standard OpenVG setup:
// 1. Get Display, Initialize, Choose Config (standard EGL dance).
// 2. Create Window Surface (or Pbuffer).
// 3. Make Current.

// Checking AmanithVG SDK specifically for "SRE" (Software Rendering Engine):
// It often supports writing to a client pointer memory buffer via 
// "vgPrivMakeCurrentMZT" or similar extensions if EGL is bypassed.
```

## Build Instructions
1.  **SDK**: Must be cloned from `github.com/Mazatech/amanithvg-sdk`.
2.  **License**: Ensure the evaluation license key is properly handled/configured if required by the code.
3.  **Link**: Link against the SRE library libraries (`libAmanithVG.a`).

## Mapping Concepts

| Benchmark IR | OpenVG API |
|---|---|
| `Path` | `VGPath` handles. `vgAppendPathData` |
| `Paint` | `VGPaint` handles. `vgSetPaint` |
| `Matrix` | `vgLoadMatrix`, `vgMultMatrix` |
| `Fill` | `vgDrawPath(path, VG_FILL_PATH)` |
| `Stroke` | `vgDrawPath(path, VG_STROKE_PATH)` |

## Notes
*   **Context**: OpenVG is state-machine based (like OpenGL).
*   **Paths**: `VGPath` objects are opaque handles.
