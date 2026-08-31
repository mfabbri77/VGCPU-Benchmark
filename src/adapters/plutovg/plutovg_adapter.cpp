// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] PlutoVG backend
// (Chapter 4)

#include "adapters/plutovg/plutovg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <plutovg.h>

#include <vector>

namespace vgcpu {

namespace {

// Set the canvas paint from an IR paint. Gradient stop colors get the same
// R<->B swap as solids (ARGB32 output; interpolation is channel-symmetric,
// so the relabeling stays exact).
plutovg_paint_t* CreatePlutoPaint(const Paint& paint) {
    if (paint.type == ir::PaintType::kSolid) {
        float r = static_cast<float>((paint.color >> 0) & 0xFF) / 255.0f;
        float g = static_cast<float>((paint.color >> 8) & 0xFF) / 255.0f;
        float b = static_cast<float>((paint.color >> 16) & 0xFF) / 255.0f;
        float a = static_cast<float>((paint.color >> 24) & 0xFF) / 255.0f;
        return plutovg_paint_create_rgba(b, g, r, a);  // R<->B swap: ARGB32 output
    }
    std::vector<plutovg_gradient_stop_t> gstops;
    gstops.reserve(paint.stops.size());
    for (const auto& s : paint.stops) {
        plutovg_gradient_stop_t gs;
        gs.offset = s.offset;
        gs.color.r = static_cast<float>((s.color >> 16) & 0xFF) / 255.0f;  // B as R
        gs.color.g = static_cast<float>((s.color >> 8) & 0xFF) / 255.0f;
        gs.color.b = static_cast<float>((s.color >> 0) & 0xFF) / 255.0f;  // R as B
        gs.color.a = static_cast<float>((s.color >> 24) & 0xFF) / 255.0f;
        gstops.push_back(gs);
    }
    if (paint.type == ir::PaintType::kLinear) {
        return plutovg_paint_create_linear_gradient(
            paint.linear_start_x, paint.linear_start_y, paint.linear_end_x, paint.linear_end_y,
            PLUTOVG_SPREAD_METHOD_PAD, gstops.data(), static_cast<int>(gstops.size()), nullptr);
    } else {
        return plutovg_paint_create_radial_gradient(
            paint.radial_center_x, paint.radial_center_y, paint.radial_radius,
            paint.radial_center_x, paint.radial_center_y, 0.0f, PLUTOVG_SPREAD_METHOD_PAD,
            gstops.data(), static_cast<int>(gstops.size()), nullptr);
    }
}

plutovg_path_t* CreatePlutoPath(const Path& path) {
    plutovg_path_t* p = plutovg_path_create();
    size_t pt_idx = 0;
    for (auto verb : path.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    plutovg_path_move_to(p, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    plutovg_path_line_to(p, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                if (pt_idx + 2 <= path.points.size() / 2) {
                    plutovg_path_quad_to(p, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1],
                                         path.points[(pt_idx + 1) * 2],
                                         path.points[(pt_idx + 1) * 2 + 1]);
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= path.points.size() / 2) {
                    plutovg_path_cubic_to(
                        p, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1],
                        path.points[(pt_idx + 1) * 2], path.points[(pt_idx + 1) * 2 + 1],
                        path.points[(pt_idx + 2) * 2], path.points[(pt_idx + 2) * 2 + 1]);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                plutovg_path_close(p);
                break;
        }
    }
    return p;
}

}  // namespace

Status PlutoVGAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    initialized_ = true;
    return Status::Ok();
}

Status PlutoVGAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_) {
        return Status::Fail("PlutoVGAdapter not initialized");
    }
    DestroyPaths();
    DestroyPaints();
    prepared_paths_.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        prepared_paths_.push_back(CreatePlutoPath(p));
    }
    prepared_paints_.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        prepared_paints_.push_back(CreatePlutoPaint(p));
    }
    return Status::Ok();
}

void PlutoVGAdapter::DestroyPaths() {
    for (auto p : prepared_paths_) {
        if (p)
            plutovg_path_destroy(p);
    }
    prepared_paths_.clear();
}

void PlutoVGAdapter::DestroyPaints() {
    for (auto p : prepared_paints_) {
        if (p)
            plutovg_paint_destroy(p);
    }
    prepared_paints_.clear();
}

void PlutoVGAdapter::Shutdown() {
    DestroyPaths();
    DestroyPaints();
    initialized_ = false;
}

AdapterInfo PlutoVGAdapter::GetInfo() const {
    return AdapterInfo{.id = "plutovg",
                       .detailed_name = "PlutoVG (CPU Software Rasterizer)",
                       .version = PLUTOVG_VERSION_STRING,
                       .is_cpu_only = true};
}

CapabilitySet PlutoVGAdapter::GetCapabilities() const {
    // PlutoVG supports all basic features
    return CapabilitySet::All();
}

namespace {

Status ExecutePlutoVGCommands(const PreparedScene& scene, const SurfaceConfig& config,
                              const std::vector<plutovg_path_t*>& paths,
                              const std::vector<plutovg_paint_t*>& paints,
                              plutovg_canvas_t* canvas) {
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    ir::StrokeCap current_stroke_cap = ir::StrokeCap::kButt;
    ir::StrokeJoin current_stroke_join = ir::StrokeJoin::kMiter;
    std::vector<float> dash_lengths;
    float dash_phase = 0.0f;

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

                float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
                float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
                float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
                float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;

                plutovg_canvas_save(canvas);
                plutovg_canvas_reset_matrix(canvas);
                plutovg_canvas_rect(canvas, 0, 0, static_cast<float>(config.width),
                                    static_cast<float>(config.height));
                plutovg_canvas_set_rgba(canvas, b, g, r, a);
                plutovg_canvas_set_operator(canvas, PLUTOVG_OPERATOR_SRC);
                plutovg_canvas_fill(canvas);
                plutovg_canvas_restore(canvas);
                plutovg_canvas_set_operator(canvas, PLUTOVG_OPERATOR_SRC_OVER);
                break;
            }
            case ir::Opcode::kSetFill: {
                if (cmd + 3 > end)
                    goto done;
                current_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_fill_rule = static_cast<ir::FillRule>(*cmd++);
                if (current_paint_id < paints.size() && paints[current_paint_id] != nullptr) {
                    plutovg_canvas_set_paint(canvas, paints[current_paint_id]);
                }
                plutovg_fill_rule_t rule = (current_fill_rule == ir::FillRule::kEvenOdd)
                                               ? PLUTOVG_FILL_RULE_EVEN_ODD
                                               : PLUTOVG_FILL_RULE_NON_ZERO;
                plutovg_canvas_set_fill_rule(canvas, rule);
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
                current_stroke_cap = ir::UnpackStrokeCap(opts);
                current_stroke_join = ir::UnpackStrokeJoin(opts);

                if (current_stroke_paint_id < paints.size() &&
                    paints[current_stroke_paint_id] != nullptr) {
                    plutovg_canvas_set_paint(canvas, paints[current_stroke_paint_id]);
                }
                plutovg_canvas_set_line_width(canvas, current_stroke_width);
                plutovg_line_cap_t cap = PLUTOVG_LINE_CAP_BUTT;
                switch (current_stroke_cap) {
                    case ir::StrokeCap::kButt:
                        cap = PLUTOVG_LINE_CAP_BUTT;
                        break;
                    case ir::StrokeCap::kRound:
                        cap = PLUTOVG_LINE_CAP_ROUND;
                        break;
                    case ir::StrokeCap::kSquare:
                        cap = PLUTOVG_LINE_CAP_SQUARE;
                        break;
                }
                plutovg_canvas_set_line_cap(canvas, cap);
                plutovg_line_join_t join = PLUTOVG_LINE_JOIN_MITER;
                switch (current_stroke_join) {
                    case ir::StrokeJoin::kMiter:
                        join = PLUTOVG_LINE_JOIN_MITER;
                        break;
                    case ir::StrokeJoin::kRound:
                        join = PLUTOVG_LINE_JOIN_ROUND;
                        break;
                    case ir::StrokeJoin::kBevel:
                        join = PLUTOVG_LINE_JOIN_BEVEL;
                        break;
                }
                plutovg_canvas_set_line_join(canvas, join);
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < paths.size() && paths[path_id] != nullptr) {
                    plutovg_canvas_fill_path(canvas, paths[path_id]);
                }
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < paths.size() && paths[path_id] != nullptr) {
                    plutovg_canvas_set_dash(canvas, dash_phase,
                                            dash_lengths.empty() ? nullptr : dash_lengths.data(),
                                            static_cast<int>(dash_lengths.size()));
                    plutovg_canvas_stroke_path(canvas, paths[path_id]);
                }
                break;
            }

            case ir::Opcode::kSetMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                plutovg_matrix_t mtx;
                plutovg_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                plutovg_canvas_set_matrix(canvas, &mtx);
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                plutovg_matrix_t mtx;
                plutovg_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                plutovg_canvas_transform(canvas, &mtx);
                break;
            }

            case ir::Opcode::kSave:
                plutovg_canvas_save(canvas);
                break;

            case ir::Opcode::kRestore:
                plutovg_canvas_restore(canvas);
                break;

            case ir::Opcode::kSetDash: {
                if (cmd + 5 > end)
                    goto done;
                uint8_t count = *cmd++;
                dash_phase = *reinterpret_cast<const float*>(cmd);
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

                plutovg_canvas_save(canvas);
                if (path_id < paths.size() && paths[path_id] != nullptr) {
                    plutovg_canvas_set_fill_rule(canvas, rule == ir::FillRule::kEvenOdd
                                                             ? PLUTOVG_FILL_RULE_EVEN_ODD
                                                             : PLUTOVG_FILL_RULE_NON_ZERO);
                    plutovg_canvas_clip_path(canvas, paths[path_id]);
                }
                break;
            }

            case ir::Opcode::kClipPop:
                plutovg_canvas_restore(canvas);
                break;

            default:
                goto done;
        }
    }

done:
    return Status::Ok();
}

}  // namespace

Status PlutoVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                              std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("PlutoVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    plutovg_surface_t* surface = plutovg_surface_create_for_data(output_buffer.data(), config.width,
                                                                 config.height, config.width * 4);
    if (!surface) {
        return Status::Fail("Failed to create PlutoVG surface");
    }

    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    if (!canvas) {
        plutovg_surface_destroy(surface);
        return Status::Fail("Failed to create PlutoVG canvas");
    }

    Status s = ExecutePlutoVGCommands(scene, config, prepared_paths_, prepared_paints_, canvas);

    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
    return s;
}

Status PlutoVGAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                       std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("PlutoVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    plutovg_surface_t* surface = plutovg_surface_create_for_data(output_buffer.data(), config.width,
                                                                 config.height, config.width * 4);
    if (!surface) {
        return Status::Fail("Failed to create PlutoVG surface");
    }

    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    if (!canvas) {
        plutovg_surface_destroy(surface);
        return Status::Fail("Failed to create PlutoVG canvas");
    }

    // Loop 1: Create all native paths + paints
    std::vector<plutovg_path_t*> paths;
    paths.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        paths.push_back(CreatePlutoPath(p));
    }

    std::vector<plutovg_paint_t*> paints;
    paints.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        paints.push_back(CreatePlutoPaint(p));
    }

    // Loop 2: Draw all
    Status s = ExecutePlutoVGCommands(scene, config, paths, paints, canvas);

    // Loop 3: Destroy all paths + paints
    for (auto p : paths) {
        if (p)
            plutovg_path_destroy(p);
    }
    paths.clear();

    for (auto p : paints) {
        if (p)
            plutovg_paint_destroy(p);
    }
    paints.clear();

    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
    return s;
}

// Explicit registration function
void RegisterPlutoVGAdapter() {
    AdapterRegistry::Instance().Register("plutovg", "PlutoVG (CPU Software Rasterizer)",
                                         []() { return std::make_unique<PlutoVGAdapter>(); });
}

}  // namespace vgcpu
