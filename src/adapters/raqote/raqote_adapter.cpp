// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-07] Rust FFI (Chapter 4) /
// [DEC-API-06] Raqote/Vello FFI (Chapter 4)

#include "adapters/raqote/raqote_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <cstdint>
#include <vector>

namespace vgcpu {

// ============================================================================
// Raqote FFI declarations (from raqote_ffi Rust crate)
// ============================================================================
extern "C" {
struct RqtSurface;
struct RqtPath;

// Surface management
RqtSurface* rqt_create(int32_t width, int32_t height);
void rqt_destroy(RqtSurface* ptr);
void rqt_clear(RqtSurface* ptr, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rqt_get_pixels(RqtSurface* ptr, uint32_t* out_buf);

// Path construction
RqtPath* rqt_path_create();
void rqt_path_destroy(RqtPath* ptr);
void rqt_path_move_to(RqtPath* ptr, float x, float y);
void rqt_path_line_to(RqtPath* ptr, float x, float y);
void rqt_path_quad_to(RqtPath* ptr, float cx, float cy, float x, float y);
void rqt_path_cubic_to(RqtPath* ptr, float c1x, float c1y, float c2x, float c2y, float x, float y);
void rqt_path_close(RqtPath* ptr);
void rqt_path_rect(RqtPath* ptr, float x, float y, float w, float h);

// Drawing operations
void rqt_fill_path(RqtSurface* surf, RqtPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                   int32_t fill_rule);
void rqt_stroke_path(RqtSurface* surf, RqtPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     float width, int32_t cap, int32_t join);
void rqt_fill_rect(RqtSurface* surf, float x, float y, float w, float h, uint8_t r, uint8_t g,
                   uint8_t b, uint8_t a);
}

namespace {

// Build a Raqote path from IR path data
RqtPath* CreateRaqotePath(const Path& path_data) {
    RqtPath* path = rqt_path_create();

    size_t pt_idx = 0;
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    rqt_path_move_to(path, path_data.points[pt_idx * 2],
                                     path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    rqt_path_line_to(path, path_data.points[pt_idx * 2],
                                     path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kQuadTo:
                if ((pt_idx + 1) * 2 + 1 < path_data.points.size()) {
                    rqt_path_quad_to(
                        path, path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1]);
                }
                pt_idx += 2;
                break;
            case ir::PathVerb::kCubicTo:
                if ((pt_idx + 2) * 2 + 1 < path_data.points.size()) {
                    rqt_path_cubic_to(
                        path, path_data.points[pt_idx * 2], path_data.points[pt_idx * 2 + 1],
                        path_data.points[(pt_idx + 1) * 2], path_data.points[(pt_idx + 1) * 2 + 1],
                        path_data.points[(pt_idx + 2) * 2], path_data.points[(pt_idx + 2) * 2 + 1]);
                }
                pt_idx += 3;
                break;
            case ir::PathVerb::kClose:
                rqt_path_close(path);
                break;
        }
    }
    return path;
}

}  // namespace

Status RaqoteAdapter::Initialize(const AdapterArgs& /*args*/) {
    initialized_ = true;
    return Status::Ok();
}

Status RaqoteAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("RaqoteAdapter not initialized");
    }
    return Status::Ok();
}

void RaqoteAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo RaqoteAdapter::GetInfo() const {
    return AdapterInfo{.id = "raqote",
                       .detailed_name = "Raqote (Rust CPU Renderer)",
                       .version = "0.8.5",
                       .is_cpu_only = true};
}

CapabilitySet RaqoteAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

Status RaqoteAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                             std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("RaqoteAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface config");

    // Create Raqote surface
    RqtSurface* surf = rqt_create(config.width, config.height);
    if (!surf)
        return Status::Fail("Failed to create Raqote surface");

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
                rqt_clear(surf, r, g, b, a);
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
                RqtPath* path = CreateRaqotePath(scene.paths[path_id]);

                // Only solid fills for now (gradients would require more FFI work)
                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;
                int32_t fill_rule = (current_fill_rule == ir::FillRule::kEvenOdd) ? 1 : 0;

                rqt_fill_path(surf, path, r, g, b, a, fill_rule);
                // Note: path is consumed by rqt_fill_path (Box::from_raw)
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
                RqtPath* path = CreateRaqotePath(scene.paths[path_id]);

                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;

                int32_t cap = 0;
                switch (current_stroke_cap) {
                    case ir::StrokeCap::kButt:
                        cap = 0;
                        break;
                    case ir::StrokeCap::kRound:
                        cap = 1;
                        break;
                    case ir::StrokeCap::kSquare:
                        cap = 2;
                        break;
                }

                int32_t join = 0;
                switch (current_stroke_join) {
                    case ir::StrokeJoin::kMiter:
                        join = 0;
                        break;
                    case ir::StrokeJoin::kRound:
                        join = 1;
                        break;
                    case ir::StrokeJoin::kBevel:
                        join = 2;
                        break;
                }

                rqt_stroke_path(surf, path, r, g, b, a, current_stroke_width, cap, join);
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                // Raqote doesn't have state stack - skip
                break;

            case ir::Opcode::kSetMatrix:
            case ir::Opcode::kConcatMatrix:
                // TODO: Implement transform support
                cmd += 24;
                break;

            default:
                break;
        }
    }

done:
    // Copy pixels to output buffer (Raqote uses ARGB order)
    rqt_get_pixels(surf, reinterpret_cast<uint32_t*>(output_buffer.data()));
    rqt_destroy(surf);

    return Status::Ok();
}

void RegisterRaqoteAdapter() {
    AdapterRegistry::Instance().Register("raqote", "Raqote (Rust CPU Renderer)",
                                         []() { return std::make_unique<RaqoteAdapter>(); });
}

}  // namespace vgcpu
