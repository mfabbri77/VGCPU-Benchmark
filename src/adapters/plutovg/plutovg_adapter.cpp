// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.5 — PlutoVG backend adapter
// Blueprint Reference: backends/plutovg.md

#include "adapters/plutovg/plutovg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <plutovg.h>

namespace vgcpu {

Status PlutoVGAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    initialized_ = true;
    return Status::Ok();
}

void PlutoVGAdapter::Shutdown() {
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

Status PlutoVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                              std::vector<uint8_t>& output_buffer) {
    if (!initialized_) {
        return Status::Fail("PlutoVGAdapter not initialized");
    }

    if (!scene.IsValid()) {
        return Status::InvalidArg("Invalid scene");
    }

    if (config.width <= 0 || config.height <= 0) {
        return Status::InvalidArg("Invalid surface configuration");
    }

    // Allocate output buffer (ARGB32, 4 bytes per pixel)
    size_t buffer_size = static_cast<size_t>(config.width) * config.height * 4;
    output_buffer.resize(buffer_size);
    std::fill(output_buffer.begin(), output_buffer.end(), 0);  // Clear to transparent

    // Create PlutoVG surface wrapping our buffer
    plutovg_surface_t* surface =
        plutovg_surface_create_for_data(output_buffer.data(), config.width, config.height,
                                        config.width * 4  // stride
        );

    if (!surface) {
        return Status::Fail("Failed to create PlutoVG surface");
    }

    // Create drawing context (canvas in v1.3.2 API)
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    if (!canvas) {
        plutovg_surface_destroy(surface);
        return Status::Fail("Failed to create PlutoVG canvas");
    }

    // Process command stream
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    // Current state
    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;

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
                float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
                float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
                float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
                float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;

                // Clear by filling entire surface
                plutovg_canvas_save(canvas);
                plutovg_canvas_reset_matrix(canvas);
                plutovg_canvas_rect(canvas, 0, 0, static_cast<float>(config.width),
                                    static_cast<float>(config.height));
                plutovg_canvas_set_rgba(canvas, r, g, b, a);
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

                const auto& path = scene.paths[path_id];
                const auto& paint = scene.paints[current_paint_id];

                // Set paint color
                if (paint.type == ir::PaintType::kSolid) {
                    float r = static_cast<float>((paint.color >> 0) & 0xFF) / 255.0f;
                    float g = static_cast<float>((paint.color >> 8) & 0xFF) / 255.0f;
                    float b = static_cast<float>((paint.color >> 16) & 0xFF) / 255.0f;
                    float a = static_cast<float>((paint.color >> 24) & 0xFF) / 255.0f;
                    plutovg_canvas_set_rgba(canvas, r, g, b, a);
                }

                // Build path
                plutovg_canvas_new_path(canvas);
                size_t pt_idx = 0;
                for (auto verb : path.verbs) {
                    switch (verb) {
                        case ir::PathVerb::kMoveTo:
                            if (pt_idx + 1 <= path.points.size() / 2) {
                                plutovg_canvas_move_to(canvas, path.points[pt_idx * 2],
                                                       path.points[pt_idx * 2 + 1]);
                                pt_idx++;
                            }
                            break;
                        case ir::PathVerb::kLineTo:
                            if (pt_idx + 1 <= path.points.size() / 2) {
                                plutovg_canvas_line_to(canvas, path.points[pt_idx * 2],
                                                       path.points[pt_idx * 2 + 1]);
                                pt_idx++;
                            }
                            break;
                        case ir::PathVerb::kQuadTo:
                            if (pt_idx + 2 <= path.points.size() / 2) {
                                plutovg_canvas_quad_to(canvas, path.points[pt_idx * 2],
                                                       path.points[pt_idx * 2 + 1],
                                                       path.points[(pt_idx + 1) * 2],
                                                       path.points[(pt_idx + 1) * 2 + 1]);
                                pt_idx += 2;
                            }
                            break;
                        case ir::PathVerb::kCubicTo:
                            if (pt_idx + 3 <= path.points.size() / 2) {
                                plutovg_canvas_cubic_to(canvas, path.points[pt_idx * 2],
                                                        path.points[pt_idx * 2 + 1],
                                                        path.points[(pt_idx + 1) * 2],
                                                        path.points[(pt_idx + 1) * 2 + 1],
                                                        path.points[(pt_idx + 2) * 2],
                                                        path.points[(pt_idx + 2) * 2 + 1]);
                                pt_idx += 3;
                            }
                            break;
                        case ir::PathVerb::kClose:
                            plutovg_canvas_close_path(canvas);
                            break;
                    }
                }

                // Set fill rule and fill
                plutovg_fill_rule_t rule = (current_fill_rule == ir::FillRule::kEvenOdd)
                                               ? PLUTOVG_FILL_RULE_EVEN_ODD
                                               : PLUTOVG_FILL_RULE_NON_ZERO;
                plutovg_canvas_set_fill_rule(canvas, rule);
                plutovg_canvas_fill(canvas);
                break;
            }

            case ir::Opcode::kSave:
                plutovg_canvas_save(canvas);
                break;

            case ir::Opcode::kRestore:
                plutovg_canvas_restore(canvas);
                break;

            default:
                // Skip unknown opcodes
                break;
        }
    }

done:
    // Cleanup
    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);

    return Status::Ok();
}

// Explicit registration function
void RegisterPlutoVGAdapter() {
    AdapterRegistry::Instance().Register("plutovg", "PlutoVG (CPU Software Rasterizer)",
                                         []() { return std::make_unique<PlutoVGAdapter>(); });
}

}  // namespace vgcpu
