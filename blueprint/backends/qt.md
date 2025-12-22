<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Qt QPainter

## 1. Version and Dependencies

*   **Version**: Qt 6.8 LTS
*   **Distribution**: System packages or Official Installer
*   **Modules**: `Qt6::Gui` (Essential), `Qt6::Core`. `Qt6::Widgets` NOT needed.
*   **License**: LGPLv3 / GPLv2 / Commercial

## 2. CMake Integration

Qt6 provides excellent CMake support.

```cmake
find_package(Qt6 COMPONENTS Gui REQUIRED)

add_library(qt_adapter STATIC ...)
target_link_libraries(qt_adapter PRIVATE Qt6::Gui)

# Note: On Linux headless, ensure "minimal" QPA plugin is available
# or pass platform arguments.
```

## 3. Initialization (CPU-Only)

We render to a `QImage`, which guarantees software rasterization regardless of the system's GPU capabilities.

```cpp
#include <QImage>
#include <QPainter>

// 1. Wrap Buffer
// Format_ARGB32_Premultiplied is standard for fast composition
QImage image(buffer.data(), width, height, stride, QImage::Format_ARGB32_Premultiplied);

// 2. Create Painter
QPainter painter(&image);
painter.setRenderHint(QPainter::Antialiasing, true);
painter.setRenderHint(QPainter::SmoothPixmapTransform, true); // if images used

// 3. Reset State
painter.eraseRect(0, 0, width, height); // Clear
```

## 4. Feature Mapping

| IR Feature | Qt API |
|---|---|
| **Path** | `QPainterPath` (moveTo, lineTo, cubicTo) |
| **Fill (Solid)** | `QBrush(QColor(...))` + `painter.fillPath` |
| **Fill (Gradient)** | `QLinearGradient` / `QRadialGradient` -> QBrush |
| **Fill Rule** | `path.setFillRule(Qt::WindingFill / Qt::OddEvenFill)` |
| **Stroke** | `QPen` (width, cap, join, dashPattern) -> `painter.strokePath` |
| **Transforms** | `painter.setTransform(QTransform(...))` |

## 5. Notes

*   **Initialization Cost**: `QGuiApplication` requires a one-time setup. This MUST be done in the adapter global init, NOT per benchmark iteration.
*   **QPA Plugin**: Run with `QT_QPA_PLATFORM=offscreen` env var to avoid X11/Wayland dependencies on CI.
    *   **Code Enforcement**: `qputenv("QT_QPA_PLATFORM", "offscreen");` before `QGuiApplication`.
