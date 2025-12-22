<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Backend Integration: Qt QPainter

## Overview
Qt QPainter is the standard 2D painting system for the Qt framework.

## CPU-Only Configuration

### Surface Creation
To enforce CPU rendering, paint onto a `QImage`. `QImage` is a hardware-independent image representation with direct pixel access.

```cpp
#include <QImage>
#include <QPainter>

// 1. Create Image (CPU buffer)
QImage image(width, height, QImage::Format_ARGB32_Premultiplied);

// 2. Initialize Painter
QPainter painter(&image);
painter.setRenderHint(QPainter::Antialiasing);

// 3. Draw
QPen pen(Qt::blue);
pen.setWidth(2);
painter.setPen(pen);
painter.drawLine(0, 0, 100, 100);

// 4. End
painter.end();
```

## Build Instructions
*   **Qt Version**: Qt 5.15 or Qt 6.x.
*   **Modules**: `Qt::Gui` is required (contains `QImage`, `QPainter`). `Qt::Widgets` is NOT required for QImage-only rendering, but often pulled in.
*   **Headless**: On Linux, use `platform=offscreen` or minimal QPA if initializing QGuiApplication is strictly required (it usually is for fonts, but maybe not for pure paths).
*   **CMake**: `find_package(Qt6 COMPONENTS Gui REQUIRED)`

## Mapping Concepts

| Benchmark IR | Qt API |
|---|---|
| `Path` | `QPainterPath` |
| `Paint` | `QBrush` (Fill), `QPen` (Stroke) |
| `Matrix` | `QTransform` |
| `Fill` | `painter.fillPath(path, brush)` |
| `Stroke` | `painter.strokePath(path, pen)` |

## Notes
*   **Overhead**: Qt is a large framework. Initializing `QGuiApplication` might be slow; exclude this from the timed section.
