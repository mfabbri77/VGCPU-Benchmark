// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#include "adapters/qt/qt_adapter.h"

#include "ir/prepared_scene.h"
#include "pal/timer.h"

#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <iostream>

namespace vgcpu {

namespace {

// Create a QPainterPath from IR path data
QPainterPath CreateQPath(const Path& path_data) {
    QPainterPath path;
    size_t pt_idx = 0;
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    path.moveTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    path.lineTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kQuadTo:
                if ((pt_idx + 1) * 2 + 1 < path_data.points.size()) {
                    path.quadTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                                path_data.points[(pt_idx + 1) * 2],
                                path_data.points[(pt_idx + 1) * 2 + 1]);
                }
                pt_idx += 2;
                break;
            case ir::PathVerb::kCubicTo:
                if ((pt_idx + 2) * 2 + 1 < path_data.points.size()) {
                    path.cubicTo(
                        path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1],
                        path_data.points[(pt_idx + 2) * 2], path_data.points[(pt_idx + 2) * 2 + 1]);
                }
                pt_idx += 3;
                break;
            case ir::PathVerb::kClose:
                path.closeSubpath();
                break;
        }
    }
    return path;
}

// Convert IR color to QColor
QColor ToQColor(uint32_t rgba) {
    return QColor::fromRgba(
        rgba);  // IR is RGBA, QColor::fromRgba expects #AARRGGBB in some formats, but Qt6 handle it
}

// Set gradient stops
template <typename T>
void SetGradientStops(T& gradient, const std::vector<ir::GradientStop>& stops) {
    for (const auto& s : stops) {
        gradient.setColorAt(static_cast<qreal>(s.offset), ToQColor(s.color));
    }
}

// Create brush from IR paint
QBrush CreateBrush(const Paint& paint) {
    if (paint.type == ir::PaintType::kSolid) {
        return QBrush(ToQColor(paint.color));
    } else if (paint.type == ir::PaintType::kLinear) {
        QLinearGradient grad(paint.linear_start_x, paint.linear_start_y, paint.linear_end_x,
                             paint.linear_end_y);
        SetGradientStops(grad, paint.stops);
        return QBrush(grad);
    } else if (paint.type == ir::PaintType::kRadial) {
        QRadialGradient grad(paint.radial_center_x, paint.radial_center_y, paint.radial_radius);
        SetGradientStops(grad, paint.stops);
        return QBrush(grad);
    }
    return QBrush();
}

}  // namespace

initialized_ = true;
return Status::Ok();
}

Status QtAdapter::Initialize(const AdapterArgs& /*args*/) {
    // QGuiApplication needs a specialized offscreen backend for CLI usage
    if (!qApp) {
        // Use static data to keep arguments alive
        static int argc = 1;
        static char script_name[] = "vgcpu-benchmark";
        static char* argv[] = {script_name, nullptr};

        // Ensure offscreen platform is used
        qputenv("QT_QPA_PLATFORM", "offscreen");

        // This will leak, but it's intentional as QGuiApplication must live for the app duration
        new QGuiApplication(argc, argv);
    }

    initialized_ = true;
    return Status::Ok();
}

Status QtAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("QtAdapter not initialized");
    }
    return Status::Ok();
}

void QtAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo QtAdapter::GetInfo() const {
    return AdapterInfo{
        .id = "qt", .detailed_name = "Qt Raster Engine", .version = "6.8.0", .is_cpu_only = true};
}

CapabilitySet QtAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

Status QtAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                         std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("QtAdapter not initialized");

    // Wrap the output buffer in a QImage
    // We use Format_ARGB32_Premultiplied which is the native fast format for Qt's raster engine
    QImage image(output_buffer.data(), config.width, config.height, config.width * 4,
                 QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Command Loop
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_fill_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;

    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    Qt::PenCapStyle current_stroke_cap = Qt::FlatCap;
    Qt::PenJoinStyle current_stroke_join = Qt::MiterJoin;

    while (cmd < end) {
        ir::Opcode opcode = static_cast<ir::Opcode>(*cmd++);

        switch (opcode) {
            case ir::Opcode::kEnd:
                goto done;

            case ir::Opcode::kClear: {
                uint32_t rgba = *reinterpret_cast<const uint32_t*>(cmd);
                cmd += 4;
                painter.fillRect(image.rect(), ToQColor(rgba));
                break;
            }

            case ir::Opcode::kSetFill: {
                current_fill_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_fill_rule = static_cast<ir::FillRule>(*cmd++);
                break;
            }

            case ir::Opcode::kSetStroke: {
                current_stroke_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_stroke_width = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                uint8_t opts = *cmd++;

                ir::StrokeCap cap = ir::UnpackStrokeCap(opts);
                ir::StrokeJoin join = ir::UnpackStrokeJoin(opts);

                switch (cap) {
                    case ir::StrokeCap::kButt:
                        current_stroke_cap = Qt::FlatCap;
                        break;
                    case ir::StrokeCap::kRound:
                        current_stroke_cap = Qt::RoundCap;
                        break;
                    case ir::StrokeCap::kSquare:
                        current_stroke_cap = Qt::SquareCap;
                        break;
                }

                switch (join) {
                    case ir::StrokeJoin::kMiter:
                        current_stroke_join = Qt::MiterJoin;
                        break;
                    case ir::StrokeJoin::kRound:
                        current_stroke_join = Qt::RoundJoin;
                        break;
                    case ir::StrokeJoin::kBevel:
                        current_stroke_join = Qt::BevelJoin;
                        break;
                }
                break;
            }

            case ir::Opcode::kFillPath: {
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size() || current_fill_paint_id >= scene.paints.size())
                    break;

                QPainterPath path = CreateQPath(scene.paths[path_id]);
                path.setFillRule(current_fill_rule == ir::FillRule::kEvenOdd ? Qt::OddEvenFill
                                                                             : Qt::WindingFill);

                painter.fillPath(path, CreateBrush(scene.paints[current_fill_paint_id]));
                break;
            }

            case ir::Opcode::kStrokePath: {
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size() || current_stroke_paint_id >= scene.paints.size())
                    break;

                QPainterPath path = CreateQPath(scene.paths[path_id]);
                QPen pen(CreateBrush(scene.paints[current_stroke_paint_id]),
                         (qreal)current_stroke_width);
                pen.setCapStyle(current_stroke_cap);
                pen.setJoinStyle(current_stroke_join);

                painter.strokePath(path, pen);
                break;
            }

            case ir::Opcode::kSave:
                painter.save();
                break;

            case ir::Opcode::kRestore:
                painter.restore();
                break;

            case ir::Opcode::kSetMatrix: {
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                painter.setTransform(QTransform(m[0], m[1], m[2], m[3], m[4], m[5]));
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                painter.setTransform(QTransform(m[0], m[1], m[2], m[3], m[4], m[5]), true);
                break;
            }
        }
    }

done:
    return Status::Ok();
}

void RegisterQtAdapter() {
    AdapterRegistry::Instance().Register("qt", "Qt Raster Engine",
                                         []() { return std::make_unique<QtAdapter>(); });
}

}  // namespace vgcpu
