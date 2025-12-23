// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] ThorVG backend
// (Chapter 4)

#include "adapters/thorvg/thorvg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <thorvg.h>

#include <cstring>
#include <vector>

namespace vgcpu {

namespace {

// Create a ThorVG shape from IR path data
std::unique_ptr<tvg::Shape> CreateShape(const Path& path_data) {
    auto shape = tvg::Shape::gen();

    size_t pt_idx = 0;
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= path_data.points.size() / 2) {
                    shape->moveTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= path_data.points.size() / 2) {
                    shape->lineTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                // ThorVG doesn't have quadTo directly, approximate with cubic
                if (pt_idx + 2 <= path_data.points.size() / 2) {
                    // Approximate: just use end point
                    shape->lineTo(path_data.points[(pt_idx + 1) * 2],
                                  path_data.points[(pt_idx + 1) * 2 + 1]);
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= path_data.points.size() / 2) {
                    shape->cubicTo(
                        path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1],
                        path_data.points[(pt_idx + 2) * 2], path_data.points[(pt_idx + 2) * 2 + 1]);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                shape->close();
                break;
        }
    }
    return shape;
}

// Apply solid fill to shape
void ApplySolidFill(tvg::Shape* shape, uint32_t color) {
    uint8_t r = (color >> 0) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    shape->fill(r, g, b, a);
}

// Apply gradient fill to shape
void ApplyGradientFill(tvg::Shape* shape, const Paint& paint) {
    if (paint.type == ir::PaintType::kLinear) {
        auto grad = tvg::LinearGradient::gen();
        grad->linear(paint.linear_start_x, paint.linear_start_y, paint.linear_end_x,
                     paint.linear_end_y);

        std::vector<tvg::Fill::ColorStop> stops;
        stops.reserve(paint.stops.size());
        for (const auto& s : paint.stops) {
            tvg::Fill::ColorStop cs;
            cs.offset = s.offset;
            cs.r = (s.color >> 0) & 0xFF;
            cs.g = (s.color >> 8) & 0xFF;
            cs.b = (s.color >> 16) & 0xFF;
            cs.a = (s.color >> 24) & 0xFF;
            stops.push_back(cs);
        }
        grad->colorStops(stops.data(), static_cast<uint32_t>(stops.size()));
        shape->fill(std::move(grad));
    } else if (paint.type == ir::PaintType::kRadial) {
        auto grad = tvg::RadialGradient::gen();
        grad->radial(paint.radial_center_x, paint.radial_center_y, paint.radial_radius);

        std::vector<tvg::Fill::ColorStop> stops;
        stops.reserve(paint.stops.size());
        for (const auto& s : paint.stops) {
            tvg::Fill::ColorStop cs;
            cs.offset = s.offset;
            cs.r = (s.color >> 0) & 0xFF;
            cs.g = (s.color >> 8) & 0xFF;
            cs.b = (s.color >> 16) & 0xFF;
            cs.a = (s.color >> 24) & 0xFF;
            stops.push_back(cs);
        }
        grad->colorStops(stops.data(), static_cast<uint32_t>(stops.size()));
        shape->fill(std::move(grad));
    }
}

}  // namespace

Status ThorVGAdapter::Initialize(const AdapterArgs& /*args*/) {
    // ThorVG v0.15.16 API: init(CanvasEngine, threads)
    if (tvg::Initializer::init(tvg::CanvasEngine::Sw, 1) != tvg::Result::Success) {
        return Status::Fail("Failed to initialize ThorVG");
    }
    initialized_ = true;
    return Status::Ok();
}

Status ThorVGAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("ThorVGAdapter not initialized");
    }
    return Status::Ok();
}

void ThorVGAdapter::Shutdown() {
    if (initialized_) {
        tvg::Initializer::term(tvg::CanvasEngine::Sw);
        initialized_ = false;
    }
}

AdapterInfo ThorVGAdapter::GetInfo() const {
    return AdapterInfo{.id = "thorvg",
                       .detailed_name = "ThorVG SW (Software Rasterizer)",
                       .version = "0.15.16",
                       .is_cpu_only = true};
}

CapabilitySet ThorVGAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

Status ThorVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                             std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("ThorVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    // Create SW canvas
    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        return Status::Fail("Failed to create ThorVG SwCanvas");
    }

    // Target the output buffer (ARGB8888 format)
    auto result =
        canvas->target(reinterpret_cast<uint32_t*>(output_buffer.data()),
                       static_cast<uint32_t>(config.width), static_cast<uint32_t>(config.width),
                       static_cast<uint32_t>(config.height), tvg::SwCanvas::ARGB8888);
    if (result != tvg::Result::Success) {
        return Status::Fail("Failed to set ThorVG canvas target");
    }

    // Command loop
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;

    // Stroke state
    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    tvg::StrokeCap current_stroke_cap = tvg::StrokeCap::Butt;
    tvg::StrokeJoin current_stroke_join = tvg::StrokeJoin::Miter;

    while (cmd < end) {
        ir::Opcode opcode = static_cast<ir::Opcode>(*cmd++);

        switch (opcode) {
            case ir::Opcode::kEnd:
                goto done;

            case ir::Opcode::kClear: {
                if (cmd + 4 > end)
                    goto done;
                uint32_t rgba = *reinterpret_cast<const uint32_t*>(cmd);
                cmd += 4;

                // Create a full-screen rectangle for clear
                auto rect = tvg::Shape::gen();
                rect->appendRect(0, 0, static_cast<float>(config.width),
                                 static_cast<float>(config.height), 0, 0);
                ApplySolidFill(rect.get(), rgba);
                canvas->push(std::move(rect));
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
                current_stroke_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_stroke_width = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                uint8_t opts = *cmd++;

                ir::StrokeCap cap = ir::UnpackStrokeCap(opts);
                ir::StrokeJoin join = ir::UnpackStrokeJoin(opts);

                switch (cap) {
                    case ir::StrokeCap::kButt:
                        current_stroke_cap = tvg::StrokeCap::Butt;
                        break;
                    case ir::StrokeCap::kRound:
                        current_stroke_cap = tvg::StrokeCap::Round;
                        break;
                    case ir::StrokeCap::kSquare:
                        current_stroke_cap = tvg::StrokeCap::Square;
                        break;
                }
                switch (join) {
                    case ir::StrokeJoin::kMiter:
                        current_stroke_join = tvg::StrokeJoin::Miter;
                        break;
                    case ir::StrokeJoin::kRound:
                        current_stroke_join = tvg::StrokeJoin::Round;
                        break;
                    case ir::StrokeJoin::kBevel:
                        current_stroke_join = tvg::StrokeJoin::Bevel;
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

                auto shape = CreateShape(scene.paths[path_id]);
                const auto& paint = scene.paints[current_paint_id];

                if (paint.type == ir::PaintType::kSolid) {
                    ApplySolidFill(shape.get(), paint.color);
                } else {
                    ApplyGradientFill(shape.get(), paint);
                }

                // Set fill rule: ThorVG uses FillRule::Winding (not NonZero)
                shape->fill(current_fill_rule == ir::FillRule::kEvenOdd ? tvg::FillRule::EvenOdd
                                                                        : tvg::FillRule::Winding);

                canvas->push(std::move(shape));
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size())
                    break;
                if (current_stroke_paint_id >= scene.paints.size())
                    break;

                auto shape = CreateShape(scene.paths[path_id]);
                const auto& paint = scene.paints[current_stroke_paint_id];

                // Configure stroke using overloaded stroke() methods
                shape->stroke(current_stroke_width);
                shape->stroke(current_stroke_cap);
                shape->stroke(current_stroke_join);

                // Stroke color
                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;
                shape->stroke(r, g, b, a);

                canvas->push(std::move(shape));
                break;
            }

            case ir::Opcode::kSave:
                // ThorVG doesn't have save/restore, skip
                break;

            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kSetMatrix:
                // TODO: Implement matrix transforms
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
    // Sync to complete rasterization
    // [API-06-05] Measurement must include work completion (sync/flush) (Chapter 4)
    canvas->draw();
    canvas->sync();

    return Status::Ok();
}

void RegisterThorVGAdapter() {
    AdapterRegistry::Instance().Register("thorvg", "ThorVG SW (Software Rasterizer)",
                                         []() { return std::make_unique<ThorVGAdapter>(); });
}

}  // namespace vgcpu
