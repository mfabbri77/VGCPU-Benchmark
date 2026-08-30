// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] Cairo backend
// (Chapter 4)

#include "adapters/cairo/cairo_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <cairo.h>

namespace vgcpu {

namespace {

// Set the cairo source from an IR paint. Returns a pattern the caller must
// destroy after drawing, or nullptr for solid colors. Gradient stop colors
// get the same R<->B swap as solids (ARGB32 output; interpolation is
// channel-symmetric, so the relabeling stays exact).
cairo_pattern_t* SetSourceFromPaint(cairo_t* cr, const Paint& paint) {
    if (paint.type == ir::PaintType::kSolid) {
        double r = static_cast<double>((paint.color >> 0) & 0xFF) / 255.0;
        double g = static_cast<double>((paint.color >> 8) & 0xFF) / 255.0;
        double b = static_cast<double>((paint.color >> 16) & 0xFF) / 255.0;
        double a = static_cast<double>((paint.color >> 24) & 0xFF) / 255.0;
        cairo_set_source_rgba(cr, b, g, r, a);  // R<->B swap: ARGB32 output
        return nullptr;
    }
    cairo_pattern_t* pat = nullptr;
    if (paint.type == ir::PaintType::kLinear) {
        pat = cairo_pattern_create_linear(paint.linear_start_x, paint.linear_start_y,
                                          paint.linear_end_x, paint.linear_end_y);
    } else {
        pat = cairo_pattern_create_radial(paint.radial_center_x, paint.radial_center_y, 0.0,
                                          paint.radial_center_x, paint.radial_center_y,
                                          paint.radial_radius);
    }
    for (const auto& s : paint.stops) {
        double r = static_cast<double>((s.color >> 0) & 0xFF) / 255.0;
        double g = static_cast<double>((s.color >> 8) & 0xFF) / 255.0;
        double b = static_cast<double>((s.color >> 16) & 0xFF) / 255.0;
        double a = static_cast<double>((s.color >> 24) & 0xFF) / 255.0;
        cairo_pattern_add_color_stop_rgba(pat, s.offset, b, g, r, a);  // R<->B swap
    }
    cairo_set_source(cr, pat);
    return pat;
}

// Rebuild the cairo current path from IR path data.
void BuildPath(cairo_t* cr, const Path& path) {
    cairo_new_path(cr);
    size_t pt_idx = 0;
    for (auto verb : path.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    cairo_move_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    cairo_line_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                // Cairo doesn't have native quad bezier, convert to cubic
                if (pt_idx + 2 <= path.points.size() / 2) {
                    double x0, y0;
                    cairo_get_current_point(cr, &x0, &y0);
                    double x1 = path.points[pt_idx * 2];
                    double y1 = path.points[pt_idx * 2 + 1];
                    double x2 = path.points[(pt_idx + 1) * 2];
                    double y2 = path.points[(pt_idx + 1) * 2 + 1];
                    // Quad to cubic: P1 = P0 + 2/3*(C - P0), P2 = P2 + 2/3*(C - P2)
                    double cx1 = x0 + (2.0 / 3.0) * (x1 - x0);
                    double cy1 = y0 + (2.0 / 3.0) * (y1 - y0);
                    double cx2 = x2 + (2.0 / 3.0) * (x1 - x2);
                    double cy2 = y2 + (2.0 / 3.0) * (y1 - y2);
                    cairo_curve_to(cr, cx1, cy1, cx2, cy2, x2, y2);
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= path.points.size() / 2) {
                    cairo_curve_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1],
                                   path.points[(pt_idx + 1) * 2], path.points[(pt_idx + 1) * 2 + 1],
                                   path.points[(pt_idx + 2) * 2],
                                   path.points[(pt_idx + 2) * 2 + 1]);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                cairo_close_path(cr);
                break;
        }
    }
}

}  // namespace

Status CairoAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    initialized_ = true;
    return Status::Ok();
}

Status CairoAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("CairoAdapter not initialized");
    }
    return Status::Ok();
}

void CairoAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo CairoAdapter::GetInfo() const {
    return AdapterInfo{.id = "cairo",
                       .detailed_name = "Cairo (Image Surface, CPU Rasterizer)",
                       .version = CAIRO_VERSION_STRING,
                       .is_cpu_only = true};
}

CapabilitySet CairoAdapter::GetCapabilities() const {
    // Cairo supports all basic features
    return CapabilitySet::All();
}

Status CairoAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                            std::vector<uint8_t>& output_buffer) {
    if (!initialized_) {
        return Status::Fail("CairoAdapter not initialized");
    }

    if (!scene.IsValid()) {
        return Status::InvalidArg("Invalid scene");
    }

    if (config.width <= 0 || config.height <= 0) {
        return Status::InvalidArg("Invalid surface configuration");
    }

    // Buffer is pre-sized by harness. Contents are undefined until kClear.
    // Cairo uses ARGB32 format with specific stride alignment
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, config.width);

    // Create Cairo surface wrapping our buffer
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        output_buffer.data(), CAIRO_FORMAT_ARGB32, config.width, config.height, stride);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        return Status::Fail("Failed to create Cairo surface");
    }

    // Create drawing context
    cairo_t* cr = cairo_create(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return Status::Fail("Failed to create Cairo context");
    }

    // Set default antialias
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    // Process command stream
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    // Current state
    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    ir::StrokeCap current_stroke_cap = ir::StrokeCap::kButt;
    ir::StrokeJoin current_stroke_join = ir::StrokeJoin::kMiter;

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

                // Extract RGBA components
                double r = static_cast<double>((rgba >> 0) & 0xFF) / 255.0;
                double g = static_cast<double>((rgba >> 8) & 0xFF) / 255.0;
                double b = static_cast<double>((rgba >> 16) & 0xFF) / 255.0;
                double a = static_cast<double>((rgba >> 24) & 0xFF) / 255.0;

                // Clear by filling entire surface
                cairo_save(cr);
                cairo_identity_matrix(cr);
                cairo_rectangle(cr, 0, 0, config.width, config.height);
                // R<->B swap: CAIRO_FORMAT_ARGB32 stores bytes B,G,R,A on
                // little-endian; the contract wants R,G,B,A. Swapped input
                // colors make the output bytes land in contract order at
                // zero per-pixel cost (blending is channel-symmetric).
                cairo_set_source_rgba(cr, b, g, r, a);
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                cairo_fill(cr);
                cairo_restore(cr);
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
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
                current_stroke_cap = ir::UnpackStrokeCap(opts);
                current_stroke_join = ir::UnpackStrokeJoin(opts);
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

                cairo_pattern_t* grad_pat = SetSourceFromPaint(cr, scene.paints[current_paint_id]);
                BuildPath(cr, scene.paths[path_id]);

                cairo_fill_rule_t rule = (current_fill_rule == ir::FillRule::kEvenOdd)
                                             ? CAIRO_FILL_RULE_EVEN_ODD
                                             : CAIRO_FILL_RULE_WINDING;
                cairo_set_fill_rule(cr, rule);
                cairo_fill(cr);
                if (grad_pat != nullptr) {
                    cairo_pattern_destroy(grad_pat);
                }
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

                cairo_pattern_t* grad_pat =
                    SetSourceFromPaint(cr, scene.paints[current_stroke_paint_id]);
                BuildPath(cr, scene.paths[path_id]);

                cairo_set_line_width(cr, current_stroke_width);
                cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
                switch (current_stroke_cap) {
                    case ir::StrokeCap::kButt:
                        cap = CAIRO_LINE_CAP_BUTT;
                        break;
                    case ir::StrokeCap::kRound:
                        cap = CAIRO_LINE_CAP_ROUND;
                        break;
                    case ir::StrokeCap::kSquare:
                        cap = CAIRO_LINE_CAP_SQUARE;
                        break;
                }
                cairo_set_line_cap(cr, cap);
                cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
                switch (current_stroke_join) {
                    case ir::StrokeJoin::kMiter:
                        join = CAIRO_LINE_JOIN_MITER;
                        break;
                    case ir::StrokeJoin::kRound:
                        join = CAIRO_LINE_JOIN_ROUND;
                        break;
                    case ir::StrokeJoin::kBevel:
                        join = CAIRO_LINE_JOIN_BEVEL;
                        break;
                }
                cairo_set_line_join(cr, join);
                cairo_stroke(cr);
                if (grad_pat != nullptr) {
                    cairo_pattern_destroy(grad_pat);
                }
                break;
            }

            case ir::Opcode::kSetMatrix: {
                if (cmd + 24 > end)
                    goto done;
                // IR [a b c d e f] = [m00 m01 m10 m11 m02 m12]:
                // cairo_matrix_init(xx, yx, xy, yy, x0, y0)
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                cairo_matrix_t mtx;
                cairo_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                cairo_set_matrix(cr, &mtx);
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                cairo_matrix_t mtx;
                cairo_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                cairo_transform(cr, &mtx);
                break;
            }

            case ir::Opcode::kSave:
                cairo_save(cr);
                break;

            case ir::Opcode::kRestore:
                cairo_restore(cr);
                break;

            default:
                // Unknown opcode: cannot know its operand size, stop parsing
                // rather than desynchronize the stream.
                goto done;
        }
    }

done:
    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return Status::Ok();
}

// Explicit registration function
void RegisterCairoAdapter() {
    AdapterRegistry::Instance().Register("cairo", "Cairo (Image Surface, CPU Rasterizer)",
                                         []() { return std::make_unique<CairoAdapter>(); });
}

}  // namespace vgcpu
