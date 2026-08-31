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
    float cur_x = 0.0f, cur_y = 0.0f;      // current point, needed for quad elevation
    float start_x = 0.0f, start_y = 0.0f;  // subpath start, restored on close
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= path_data.points.size() / 2) {
                    cur_x = start_x = path_data.points[pt_idx * 2];
                    cur_y = start_y = path_data.points[pt_idx * 2 + 1];
                    shape->moveTo(cur_x, cur_y);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= path_data.points.size() / 2) {
                    cur_x = path_data.points[pt_idx * 2];
                    cur_y = path_data.points[pt_idx * 2 + 1];
                    shape->lineTo(cur_x, cur_y);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                // ThorVG has no quadTo: exact degree elevation to cubic.
                // c1 = p0 + 2/3 (q - p0), c2 = p1 + 2/3 (q - p1).
                if (pt_idx + 2 <= path_data.points.size() / 2) {
                    const float qx = path_data.points[pt_idx * 2];
                    const float qy = path_data.points[pt_idx * 2 + 1];
                    const float ex = path_data.points[(pt_idx + 1) * 2];
                    const float ey = path_data.points[(pt_idx + 1) * 2 + 1];
                    const float c1x = cur_x + 2.0f / 3.0f * (qx - cur_x);
                    const float c1y = cur_y + 2.0f / 3.0f * (qy - cur_y);
                    const float c2x = ex + 2.0f / 3.0f * (qx - ex);
                    const float c2y = ey + 2.0f / 3.0f * (qy - ey);
                    shape->cubicTo(c1x, c1y, c2x, c2y, ex, ey);
                    cur_x = ex;
                    cur_y = ey;
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= path_data.points.size() / 2) {
                    cur_x = path_data.points[(pt_idx + 2) * 2];
                    cur_y = path_data.points[(pt_idx + 2) * 2 + 1];
                    shape->cubicTo(path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                                   path_data.points[(pt_idx + 1) * 2],
                                   path_data.points[(pt_idx + 1) * 2 + 1], cur_x, cur_y);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                cur_x = start_x;
                cur_y = start_y;
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

Status ThorVGAdapter::Initialize(const AdapterArgs& args) {
    // ThorVG v0.15.16 API: init(CanvasEngine, threads) where `threads` is
    // the number of ASYNC WORKER threads in ThorVG's task scheduler, on top
    // of the calling thread; 0 = fully synchronous on the caller. The
    // harness's --threads N is a total budget, so pass N-1 workers.
    // Fixed 2026-08-30: this was hardcoded to 1 worker, so in the
    // single-thread benchmark column ThorVG alone rendered on a second
    // thread (visible as wall < cpu in every report).
    unsigned workers = args.thread_count > 1 ? static_cast<unsigned>(args.thread_count - 1) : 0;
    if (tvg::Initializer::init(tvg::CanvasEngine::Sw, workers) != tvg::Result::Success) {
        return Status::Fail("Failed to initialize ThorVG");
    }
    initialized_ = true;
    return Status::Ok();
}

Status ThorVGAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_) {
        return Status::Fail("ThorVGAdapter not initialized");
    }
    prepared_shapes_.clear();
    prepared_shapes_.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        prepared_shapes_.push_back(CreateShape(p));
    }
    return Status::Ok();
}

void ThorVGAdapter::Shutdown() {
    prepared_shapes_.clear();
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

namespace {

template <typename ShapeGetter>
Status ExecuteThorVGCommands(const PreparedScene& scene, const SurfaceConfig& config,
                             ShapeGetter&& get_shape, tvg::SwCanvas* canvas) {
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;

    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    tvg::StrokeCap current_stroke_cap = tvg::StrokeCap::Butt;
    tvg::StrokeJoin current_stroke_join = tvg::StrokeJoin::Miter;
    std::vector<float> dash_lengths;
    struct ClipEntry {
        uint16_t path_id;
        ir::FillRule rule;
    };
    std::vector<ClipEntry> clip_stack;
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

                if (current_paint_id >= scene.paints.size())
                    break;

                auto shape = get_shape(path_id);
                if (!shape)
                    break;

                const auto& paint = scene.paints[current_paint_id];
                if (paint.type == ir::PaintType::kSolid) {
                    ApplySolidFill(shape.get(), paint.color);
                } else {
                    ApplyGradientFill(shape.get(), paint);
                }

                shape->fill(current_fill_rule == ir::FillRule::kEvenOdd ? tvg::FillRule::EvenOdd
                                                                        : tvg::FillRule::Winding);

                if (!clip_stack.empty()) {
                    const auto& top_clip = clip_stack.back();
                    auto clipper = get_shape(top_clip.path_id);
                    if (clipper) {
                        clipper->fill(top_clip.rule == ir::FillRule::kEvenOdd
                                          ? tvg::FillRule::EvenOdd
                                          : tvg::FillRule::Winding);
                        shape->clip(std::move(clipper));
                    }
                }

                canvas->push(std::move(shape));
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (current_stroke_paint_id >= scene.paints.size())
                    break;

                auto shape = get_shape(path_id);
                if (!shape)
                    break;

                const auto& paint = scene.paints[current_stroke_paint_id];
                shape->stroke(current_stroke_width);
                shape->stroke(current_stroke_cap);
                shape->stroke(current_stroke_join);

                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;
                shape->stroke(r, g, b, a);
                if (!dash_lengths.empty()) {
                    shape->stroke(dash_lengths.data(), static_cast<uint32_t>(dash_lengths.size()));
                }

                if (!clip_stack.empty()) {
                    const auto& top_clip = clip_stack.back();
                    auto clipper = get_shape(top_clip.path_id);
                    if (clipper) {
                        clipper->fill(top_clip.rule == ir::FillRule::kEvenOdd
                                          ? tvg::FillRule::EvenOdd
                                          : tvg::FillRule::Winding);
                        shape->clip(std::move(clipper));
                    }
                }

                canvas->push(std::move(shape));
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kSetDash: {
                if (cmd + 5 > end)
                    goto done;
                uint8_t count = *cmd++;
                cmd += 4;
                if (cmd + 4 * count > end)
                    goto done;
                const float* lengths = reinterpret_cast<const float*>(cmd);
                cmd += 4 * count;
                dash_lengths.assign(lengths, lengths + count);
                break;
            }

            case ir::Opcode::kClipPush: {
                if (cmd + 3 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                ir::FillRule rule = static_cast<ir::FillRule>(*cmd++);
                clip_stack.push_back({path_id, rule});
                break;
            }

            case ir::Opcode::kClipPop:
                if (!clip_stack.empty())
                    clip_stack.pop_back();
                break;

            case ir::Opcode::kSetMatrix:
            case ir::Opcode::kConcatMatrix:
                cmd += 24;
                break;

            default:
                goto done;
        }
    }

done:
    canvas->draw();
    canvas->sync();
    return Status::Ok();
}

}  // namespace

Status ThorVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                             std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("ThorVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        return Status::Fail("Failed to create ThorVG SwCanvas");
    }

    auto result =
        canvas->target(reinterpret_cast<uint32_t*>(output_buffer.data()),
                       static_cast<uint32_t>(config.width), static_cast<uint32_t>(config.width),
                       static_cast<uint32_t>(config.height), tvg::SwCanvas::ABGR8888);
    if (result != tvg::Result::Success) {
        return Status::Fail("Failed to set ThorVG canvas target");
    }

    auto get_shape = [&](uint16_t path_id) -> std::unique_ptr<tvg::Shape> {
        if (path_id >= prepared_shapes_.size() || !prepared_shapes_[path_id])
            return nullptr;
        auto dup = prepared_shapes_[path_id]->duplicate();
        return std::unique_ptr<tvg::Shape>(static_cast<tvg::Shape*>(dup));
    };
    return ExecuteThorVGCommands(scene, config, get_shape, canvas.get());
}

Status ThorVGAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                      std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("ThorVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        return Status::Fail("Failed to create ThorVG SwCanvas");
    }

    auto result =
        canvas->target(reinterpret_cast<uint32_t*>(output_buffer.data()),
                       static_cast<uint32_t>(config.width), static_cast<uint32_t>(config.width),
                       static_cast<uint32_t>(config.height), tvg::SwCanvas::ABGR8888);
    if (result != tvg::Result::Success) {
        return Status::Fail("Failed to set ThorVG canvas target");
    }

    // Loop 1: Create all native shapes
    std::vector<std::unique_ptr<tvg::Shape>> shapes;
    shapes.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        shapes.push_back(CreateShape(p));
    }

    auto get_shape = [&](uint16_t path_id) -> std::unique_ptr<tvg::Shape> {
        if (path_id >= shapes.size() || !shapes[path_id])
            return nullptr;
        auto dup = shapes[path_id]->duplicate();
        return std::unique_ptr<tvg::Shape>(static_cast<tvg::Shape*>(dup));
    };
    // Loop 2: Draw all
    Status s = ExecuteThorVGCommands(scene, config, get_shape, canvas.get());

    // Loop 3: Destroy all
    shapes.clear();
    return s;
}

void RegisterThorVGAdapter() {
    AdapterRegistry::Instance().Register("thorvg", "ThorVG SW (Software Rasterizer)",
                                         []() { return std::make_unique<ThorVGAdapter>(); });
}

}  // namespace vgcpu
