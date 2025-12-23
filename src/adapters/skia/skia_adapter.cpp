// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#include "adapters/skia/skia_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

// Skia Includes
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradientShader.h"

#include <vector>

namespace vgcpu {

namespace {

SkColor ConvertColor(uint32_t c) {
    // IR color is RGBA8 (A is high byte: 0xAABBGGRR in Little Endian u32? No wait.)
    // IR Format says: "RGBA8 premultiplied (default: opaque black)"
    // Typically u32 color in memory:
    // If we assume Little Endian and 0xAABBGGRR (Intel/ARM), then:
    // ir::Paint code says: r | (g << 8) | (b << 16) | (a << 24)
    // SkColor is 0xAARRGGBB (on most configs? No, Skia usually defines SkColor as ARGB).
    // SkColorSetARGB(a, r, g, b) does packing.
    //
    // Let's unpack specifically to be safe.
    uint8_t r = (c >> 0) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = (c >> 16) & 0xFF;
    uint8_t a = (c >> 24) & 0xFF;
    return SkColorSetARGB(a, r, g, b);
}

SkPath CreatePath(const Path& irGraph) {
    SkPath path;
    size_t pt_idx = 0;

    for (auto verb : irGraph.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= irGraph.points.size() / 2) {
                    path.moveTo(irGraph.points[pt_idx * 2], irGraph.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= irGraph.points.size() / 2) {
                    path.lineTo(irGraph.points[pt_idx * 2], irGraph.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                if (pt_idx + 2 <= irGraph.points.size() / 2) {
                    path.quadTo(irGraph.points[pt_idx * 2], irGraph.points[pt_idx * 2 + 1],
                                irGraph.points[(pt_idx + 1) * 2],
                                irGraph.points[(pt_idx + 1) * 2 + 1]);
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= irGraph.points.size() / 2) {
                    path.cubicTo(
                        irGraph.points[pt_idx * 2], irGraph.points[pt_idx * 2 + 1],
                        irGraph.points[(pt_idx + 1) * 2], irGraph.points[(pt_idx + 1) * 2 + 1],
                        irGraph.points[(pt_idx + 2) * 2], irGraph.points[(pt_idx + 2) * 2 + 1]);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                path.close();
                break;
        }
    }
    return path;
}

void ApplyPaint(SkPaint& skPaint, const Paint& irPaint) {
    skPaint.setAntiAlias(true);

    if (irPaint.type == ir::PaintType::kSolid) {
        skPaint.setColor(ConvertColor(irPaint.color));
        skPaint.setShader(nullptr);
    } else if (irPaint.type == ir::PaintType::kLinear) {
        std::vector<SkColor> colors;
        std::vector<SkScalar> pos;
        colors.reserve(irPaint.stops.size());
        pos.reserve(irPaint.stops.size());

        for (const auto& s : irPaint.stops) {
            colors.push_back(ConvertColor(s.color));
            pos.push_back(s.offset);
        }

        SkPoint pts[2] = {SkPoint::Make(irPaint.linear_start_x, irPaint.linear_start_y),
                          SkPoint::Make(irPaint.linear_end_x, irPaint.linear_end_y)};

        sk_sp<SkShader> shader = SkGradientShader::MakeLinear(
            pts, colors.data(), pos.data(), static_cast<int>(colors.size()), SkTileMode::kClamp);
        skPaint.setShader(shader);
    } else if (irPaint.type == ir::PaintType::kRadial) {
        std::vector<SkColor> colors;
        std::vector<SkScalar> pos;
        colors.reserve(irPaint.stops.size());
        pos.reserve(irPaint.stops.size());

        for (const auto& s : irPaint.stops) {
            colors.push_back(ConvertColor(s.color));
            pos.push_back(s.offset);
        }

        SkPoint center = SkPoint::Make(irPaint.radial_center_x, irPaint.radial_center_y);

        sk_sp<SkShader> shader =
            SkGradientShader::MakeRadial(center, irPaint.radial_radius, colors.data(), pos.data(),
                                         static_cast<int>(colors.size()), SkTileMode::kClamp);
        skPaint.setShader(shader);
    }
}

}  // namespace

Status SkiaAdapter::Initialize(const AdapterArgs& /*args*/) {
    initialized_ = true;
    return Status::Ok();
}

Status SkiaAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("SkiaAdapter not initialized");
    }
    return Status::Ok();
}

void SkiaAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo SkiaAdapter::GetInfo() const {
    return AdapterInfo{.id = "skia",
                       .detailed_name = "Skia (CPU Raster)",
                       .version = "m124 (Aseprite build)",
                       .is_cpu_only = true};
}

CapabilitySet SkiaAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

Status SkiaAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("SkiaAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");

    // Buffer is pre-sized by harness. Contents are undefined until kClear.

    // Create SkSurface wrapping our buffer
    SkImageInfo info =
        SkImageInfo::Make(config.width, config.height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);

    sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(info, output_buffer.data(), config.width * 4);

    if (!surface) {
        return Status::Fail("Failed to create SkSurface");
    }

    SkCanvas* canvas = surface->getCanvas();

    // Command loop
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;

    // Stroke state
    struct StrokeState {
        uint16_t paint_id = 0;
        float width = 1.0f;
        SkPaint::Cap cap = SkPaint::kButt_Cap;
        SkPaint::Join join = SkPaint::kMiter_Join;
    } current_stroke;

    while (cmd < end) {
        ir::Opcode opcode = static_cast<ir::Opcode>(*cmd++);

        switch (opcode) {
            case ir::Opcode::kEnd:
                goto done;

            case ir::Opcode::kClear: {
                if (cmd + 4 > end)
                    goto done;
                uint32_t c = *reinterpret_cast<const uint32_t*>(cmd);
                cmd += 4;
                canvas->clear(ConvertColor(c));
                break;
            }

            case ir::Opcode::kSetFill: {
                if (cmd + 3 > end)
                    goto done;
                current_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_fill_rule = static_cast<ir::FillRule>(*cmd++);
                break;
            }

            case ir::Opcode::kSetStroke: {
                if (cmd + 7 > end)
                    goto done;
                current_stroke.paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_stroke.width = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                uint8_t opts = *cmd++;

                ir::StrokeCap c = ir::UnpackStrokeCap(opts);
                ir::StrokeJoin j = ir::UnpackStrokeJoin(opts);

                switch (c) {
                    case ir::StrokeCap::kButt:
                        current_stroke.cap = SkPaint::kButt_Cap;
                        break;
                    case ir::StrokeCap::kRound:
                        current_stroke.cap = SkPaint::kRound_Cap;
                        break;
                    case ir::StrokeCap::kSquare:
                        current_stroke.cap = SkPaint::kSquare_Cap;
                        break;
                }
                switch (j) {
                    case ir::StrokeJoin::kMiter:
                        current_stroke.join = SkPaint::kMiter_Join;
                        break;
                    case ir::StrokeJoin::kRound:
                        current_stroke.join = SkPaint::kRound_Join;
                        break;
                    case ir::StrokeJoin::kBevel:
                        current_stroke.join = SkPaint::kBevel_Join;
                        break;
                }
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size())
                    break;
                if (current_paint_id >= scene.paints.size())
                    break;

                SkPaint paint;
                paint.setStyle(SkPaint::kFill_Style);
                ApplyPaint(paint, scene.paints[current_paint_id]);

                SkPath path = CreatePath(scene.paths[path_id]);
                path.setFillType(current_fill_rule == ir::FillRule::kEvenOdd
                                     ? SkPathFillType::kEvenOdd
                                     : SkPathFillType::kWinding);

                canvas->drawPath(path, paint);
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size())
                    break;
                if (current_stroke.paint_id >= scene.paints.size())
                    break;

                SkPaint paint;
                paint.setStyle(SkPaint::kStroke_Style);
                paint.setStrokeWidth(current_stroke.width);
                paint.setStrokeCap(current_stroke.cap);
                paint.setStrokeJoin(current_stroke.join);
                ApplyPaint(paint, scene.paints[current_stroke.paint_id]);

                SkPath path = CreatePath(scene.paths[path_id]);
                canvas->drawPath(path, paint);
                break;
            }

            case ir::Opcode::kSave:
                canvas->save();
                break;

            case ir::Opcode::kRestore:
                canvas->restore();
                break;

            case ir::Opcode::kSetMatrix:
                // TODO: Implement matrix
                cmd += 24;
                break;

            case ir::Opcode::kConcatMatrix:
                cmd += 24;
                break;

            default:
                break;
        }
    }

done:
    return Status::Ok();
}

void RegisterSkiaAdapter() {
    AdapterRegistry::Instance().Register("skia", "Skia (CPU Raster)",
                                         []() { return std::make_unique<SkiaAdapter>(); });
}

}  // namespace vgcpu
