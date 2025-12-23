// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-07] Rust FFI (Chapter 4) /
// [DEC-API-06] Raqote/Vello FFI (Chapter 4)

#include "adapters/vello/vello_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <cstdint>
#include <vector>

namespace vgcpu {

// ============================================================================
// Vello FFI declarations (from vello_ffi Rust crate)
// ============================================================================
extern "C" {
struct VloSurface;
struct VloPath;

// Surface management
VloSurface* vlo_create(int32_t width, int32_t height);
void vlo_destroy(VloSurface* ptr);
void vlo_clear(VloSurface* ptr, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void vlo_get_pixels(VloSurface* ptr, uint32_t* out_buf);

// Path construction
VloPath* vlo_path_create();
void vlo_path_destroy(VloPath* ptr);
void vlo_path_move_to(VloPath* ptr, float x, float y);
void vlo_path_line_to(VloPath* ptr, float x, float y);
void vlo_path_quad_to(VloPath* ptr, float cx, float cy, float x, float y);
void vlo_path_cubic_to(VloPath* ptr, float c1x, float c1y, float c2x, float c2y, float x, float y);
void vlo_path_close(VloPath* ptr);

// Drawing operations
void vlo_fill_path(VloSurface* surf, VloPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                   bool even_odd);
void vlo_stroke_path(VloSurface* surf, VloPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     float width, int32_t cap, int32_t join);
void vlo_fill_rect(VloSurface* surf, float x, float y, float w, float h, uint8_t r, uint8_t g,
                   uint8_t b, uint8_t a);
}

namespace {

// Build a Vello path from IR path data
VloPath* CreateVelloPath(const Path& path_data) {
    VloPath* path = vlo_path_create();

    size_t pt_idx = 0;
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    vlo_path_move_to(path, path_data.points[pt_idx * 2],
                                     path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    vlo_path_line_to(path, path_data.points[pt_idx * 2],
                                     path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kQuadTo:
                if ((pt_idx + 1) * 2 + 1 < path_data.points.size()) {
                    vlo_path_quad_to(
                        path, path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1]);
                }
                pt_idx += 2;
                break;
            case ir::PathVerb::kCubicTo:
                if ((pt_idx + 2) * 2 + 1 < path_data.points.size()) {
                    vlo_path_cubic_to(
                        path, path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1],
                        path_data.points[(pt_idx + 2) * 2], path_data.points[(pt_idx + 2) * 2 + 1]);
                }
                pt_idx += 3;
                break;
            case ir::PathVerb::kClose:
                vlo_path_close(path);
                break;
        }
    }
    return path;
}

}  // namespace

Status VelloAdapter::Initialize(const AdapterArgs& /*args*/) {
    initialized_ = true;
    return Status::Ok();
}

Status VelloAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("VelloAdapter not initialized");
    }
    return Status::Ok();
}

void VelloAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo VelloAdapter::GetInfo() const {
    return AdapterInfo{.id = "vello",
                       .detailed_name = "vello_cpu (Experimental Rust Renderer)",
                       .version = "0.0.4",
                       .is_cpu_only = true};
}

CapabilitySet VelloAdapter::GetCapabilities() const {
    // vello_cpu 0.0.4 is very limited in its public API
    CapabilitySet caps;
    caps.supports_nonzero = true;
    caps.supports_evenodd = false;  // Not explicitly exposed in 0.0.4 FFI
    caps.supports_linear_gradient = false;
    caps.supports_radial_gradient = false;
    caps.supports_clipping = false;
    caps.supports_dashes = false;
    return caps;
}

Status VelloAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                            std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("VelloAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface config");

    // Create Vello surface
    VloSurface* surf = vlo_create(config.width, config.height);
    if (!surf)
        return Status::Fail("Failed to create Vello surface");

    // Command loop state
    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    ir::StrokeCap current_stroke_cap = ir::StrokeCap::kButt;
    ir::StrokeJoin current_stroke_join = ir::StrokeJoin::kMiter;

    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

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
                uint8_t r = (rgba >> 0) & 0xFF;
                uint8_t g = (rgba >> 8) & 0xFF;
                uint8_t b = (rgba >> 16) & 0xFF;
                uint8_t a = (rgba >> 24) & 0xFF;
                vlo_clear(surf, r, g, b, a);
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

                if (path_id >= scene.paths.size() || current_paint_id >= scene.paints.size())
                    break;

                const auto& paint = scene.paints[current_paint_id];
                VloPath* path = CreateVelloPath(scene.paths[path_id]);

                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;
                bool even_odd = (current_fill_rule == ir::FillRule::kEvenOdd);

                vlo_fill_path(surf, path, r, g, b, a, even_odd);
                vlo_path_destroy(path);
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= scene.paths.size() || current_stroke_paint_id >= scene.paints.size())
                    break;

                const auto& paint = scene.paints[current_stroke_paint_id];
                VloPath* path = CreateVelloPath(scene.paths[path_id]);

                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;

                vlo_stroke_path(surf, path, r, g, b, a, current_stroke_width,
                                (int)current_stroke_cap, (int)current_stroke_join);
                vlo_path_destroy(path);
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                // vello_cpu doesn't have state stack exposed this way
                break;

            case ir::Opcode::kSetMatrix:
            case ir::Opcode::kConcatMatrix:
                // TODO: Implement transform support if vello_cpu supports it
                cmd += 24;
                break;

            default:
                break;
        }
    }

done:
    // Copy pixels to output buffer (vello_cpu uses ARGB order internally)
    vlo_get_pixels(surf, reinterpret_cast<uint32_t*>(output_buffer.data()));
    vlo_destroy(surf);

    return Status::Ok();
}

void RegisterVelloAdapter() {
    AdapterRegistry::Instance().Register("vello", "vello_cpu (Experimental Rust Renderer)",
                                         []() { return std::make_unique<VelloAdapter>(); });
}

}  // namespace vgcpu
