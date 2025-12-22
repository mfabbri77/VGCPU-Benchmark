<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Qt Raster Engine

> **CPU-Only**: This backend uses Qt's raster paint engine with `QImage` for pure CPU software rasterization. `QPixmap` (which may use GPU) is NOT used.

## 1. Version and Dependencies

*   **Version**: Qt 6.9.3 (or any 6.x LTS)
*   **Distribution**: System packages or Official Installer
*   **Modules**: `Qt6::Gui` (Essential), `Qt6::Core`. `Qt6::Widgets` NOT needed.
*   **License**: LGPLv3 / GPLv2 / Commercial
*   **Rendering**: Pure CPU software rasterization via Qt's raster paint engine

> [!IMPORTANT]
> **System Dependency**: Unlike other backends (PlutoVG, Blend2D, Skia), Qt **cannot be integrated via CMake FetchContent**. This is an official Qt limitation documented at [doc.qt.io](https://doc.qt.io/qt-6/cmake-get-started.html). Qt must be pre-installed on the system.

### Installation

```bash
# macOS (Homebrew)
brew install qt

# Linux (Debian/Ubuntu)
sudo apt install qt6-base-dev

# Windows
# Use Qt Online Installer or vcpkg
```

## 2. CMake Integration

Qt6 provides excellent CMake support. The build system gracefully handles missing Qt by disabling the backend automatically.

```cmake
# Qt requires system installation - cannot use FetchContent
if(ENABLE_QT)
    find_package(Qt6 COMPONENTS Gui QUIET)
    if(Qt6Gui_FOUND)
        message(STATUS "Qt6 Gui found (Version: ${Qt6Gui_VERSION})")
    else()
        message(WARNING "Qt6 Gui not found. Qt backend will be disabled.")
        set(ENABLE_QT OFF CACHE BOOL "" FORCE)
    endif()
endif()

# Linking
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
