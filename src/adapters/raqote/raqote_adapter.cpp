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

// ============================================================================
// Raqote FFI declarations (from raqote_ffi Rust crate)
// ============================================================================
extern "C" {
struct RqtSurface;
struct RqtPath;
struct RqtPathBuf;
struct RqtSourceBuf;
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

RqtPathBuf* rqt_path_build(RqtPath* ptr);
void rqt_path_buf_destroy(RqtPathBuf* ptr);

// Drawing operations
void rqt_fill_path(RqtSurface* surf, RqtPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                   int32_t fill_rule);
void rqt_fill_path_gradient(RqtSurface* surf, RqtPath* path, int32_t kind, float x0, float y0,
                            float x1, float y1, const float* offsets, const uint32_t* colors,
                            int32_t nstops, int32_t fill_rule);
void rqt_stroke_path(RqtSurface* surf, RqtPath* path, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     float width, int32_t cap, int32_t join, const float* dashes, int32_t ndash,
                     float dash_phase);
void rqt_clip_push(RqtSurface* surf, RqtPath* path);
void rqt_clip_pop(RqtSurface* surf);
void rqt_fill_rect(RqtSurface* surf, float x, float y, float w, float h, uint8_t r, uint8_t g,
                   uint8_t b, uint8_t a);

void rqt_draw_fill_path_buf(RqtSurface* surf, const RqtPathBuf* path, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a, int32_t fill_rule);
void rqt_draw_fill_path_buf_gradient(RqtSurface* surf, const RqtPathBuf* path, int32_t kind,
                                     float x0, float y0, float x1, float y1, const float* offsets,
                                     const uint32_t* colors, int32_t nstops, int32_t fill_rule);
void rqt_draw_stroke_path_buf(RqtSurface* surf, const RqtPathBuf* path, uint8_t r, uint8_t g,
                              uint8_t b, uint8_t a, float width, int32_t cap, int32_t join,
                              const float* dashes, int32_t ndash, float dash_phase);
void rqt_clip_push_buf(RqtSurface* surf, const RqtPathBuf* path);

RqtSourceBuf* rqt_source_create_solid(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
RqtSourceBuf* rqt_source_create_gradient(int32_t kind, float x0, float y0, float x1, float y1,
                                         const float* offsets, const uint32_t* colors,
                                         int32_t nstops);
void rqt_source_destroy(RqtSourceBuf* ptr);
void rqt_draw_fill_with_source(RqtSurface* surf, const RqtPathBuf* path, const RqtSourceBuf* src,
                               int32_t fill_rule);
void rqt_draw_stroke_with_source(RqtSurface* surf, const RqtPathBuf* path, const RqtSourceBuf* src,
                                 float width, int32_t cap, int32_t join, const float* dashes,
                                 int32_t ndash, float dash_phase);
}

namespace vgcpu {
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

RqtPathBuf* BuildRaqotePathBuf(const Path& path_data) {
    RqtPath* pb = CreateRaqotePath(path_data);
    return rqt_path_build(pb);
}

RqtSourceBuf* CreateRaqoteSource(const Paint& paint) {
    if (paint.type == ir::PaintType::kSolid) {
        uint8_t r = (paint.color >> 0) & 0xFF;
        uint8_t g = (paint.color >> 8) & 0xFF;
        uint8_t b = (paint.color >> 16) & 0xFF;
        uint8_t a = (paint.color >> 24) & 0xFF;
        return rqt_source_create_solid(b, g, r, a);
    }
    std::vector<float> offsets;
    std::vector<uint32_t> colors;
    offsets.reserve(paint.stops.size());
    colors.reserve(paint.stops.size());
    for (const auto& s : paint.stops) {
        offsets.push_back(s.offset);
        uint32_t c = s.color;
        uint32_t swapped = (c & 0xFF00FF00u) | ((c & 0xFFu) << 16) | ((c >> 16) & 0xFFu);
        colors.push_back(swapped);
    }
    if (paint.type == ir::PaintType::kLinear) {
        return rqt_source_create_gradient(0, paint.linear_start_x, paint.linear_start_y,
                                          paint.linear_end_x, paint.linear_end_y, offsets.data(),
                                          colors.data(), static_cast<int32_t>(offsets.size()));
    } else {
        return rqt_source_create_gradient(1, paint.radial_center_x, paint.radial_center_y,
                                          paint.radial_radius, 0.0f, offsets.data(), colors.data(),
                                          static_cast<int32_t>(offsets.size()));
    }
}

}  // namespace

Status RaqoteAdapter::Initialize(const AdapterArgs& /*args*/) {
    initialized_ = true;
    return Status::Ok();
}

Status RaqoteAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_) {
        return Status::Fail("RaqoteAdapter not initialized");
    }
    DestroyPaths();
    DestroyPaints();
    prepared_paths_.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        prepared_paths_.push_back(BuildRaqotePathBuf(p));
    }
    prepared_sources_.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        prepared_sources_.push_back(CreateRaqoteSource(p));
    }
    return Status::Ok();
}

void RaqoteAdapter::DestroyPaths() {
    for (auto p : prepared_paths_) {
        if (p)
            rqt_path_buf_destroy(p);
    }
    prepared_paths_.clear();
}

void RaqoteAdapter::DestroyPaints() {
    for (auto p : prepared_sources_) {
        if (p)
            rqt_source_destroy(p);
    }
    prepared_sources_.clear();
}

void RaqoteAdapter::Shutdown() {
    DestroyPaths();
    DestroyPaints();
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
namespace {

Status ExecuteRaqoteCommands(const PreparedScene& scene, const SurfaceConfig& /*config*/,
                             const std::vector<RqtPathBuf*>& paths,
                             const std::vector<RqtSourceBuf*>& sources, RqtSurface* surf) {
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

                uint8_t r = (rgba >> 0) & 0xFF;
                uint8_t g = (rgba >> 8) & 0xFF;
                uint8_t b = (rgba >> 16) & 0xFF;
                uint8_t a = (rgba >> 24) & 0xFF;

                rqt_clear(surf, b, g, r, a);
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

                if (path_id >= paths.size() || current_paint_id >= scene.paints.size())
                    break;
                if (paths[path_id] == nullptr)
                    break;

                const auto& paint = scene.paints[current_paint_id];
                uint8_t r = (paint.color >> 0) & 0xFF;
                uint8_t g = (paint.color >> 8) & 0xFF;
                uint8_t b = (paint.color >> 16) & 0xFF;
                uint8_t a = (paint.color >> 24) & 0xFF;

                int32_t fill_rule = (current_fill_rule == ir::FillRule::kEvenOdd) ? 1 : 0;
                if (current_paint_id < sources.size() && sources[current_paint_id] != nullptr) {
                    rqt_draw_fill_with_source(surf, paths[path_id], sources[current_paint_id],
                                              fill_rule);
                }
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= paths.size() || paths[path_id] == nullptr)
                    break;
                if (current_stroke_paint_id >= sources.size() ||
                    sources[current_stroke_paint_id] == nullptr)
                    break;

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

                rqt_draw_stroke_with_source(surf, paths[path_id], sources[current_stroke_paint_id],
                                            current_stroke_width, cap, join,
                                            dash_lengths.empty() ? nullptr : dash_lengths.data(),
                                            static_cast<int32_t>(dash_lengths.size()), dash_phase);
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kSetMatrix:
            case ir::Opcode::kConcatMatrix:
                cmd += 24;
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
                cmd += 1;  // rule

                if (path_id < paths.size() && paths[path_id] != nullptr) {
                    rqt_clip_push_buf(surf, paths[path_id]);
                }
                break;
            }

            case ir::Opcode::kClipPop:
                rqt_clip_pop(surf);
                break;

            default:
                goto done;
        }
    }

done:
    return Status::Ok();
}

}  // namespace

Status RaqoteAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                             std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("RaqoteAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface config");

    RqtSurface* surf = rqt_create(config.width, config.height);
    if (!surf)
        return Status::Fail("Failed to create Raqote surface");
    Status s = ExecuteRaqoteCommands(scene, config, prepared_paths_, prepared_sources_, surf);

    rqt_get_pixels(surf, reinterpret_cast<uint32_t*>(output_buffer.data()));
    rqt_destroy(surf);
    return s;
}

Status RaqoteAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                      std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("RaqoteAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface config");

    RqtSurface* surf = rqt_create(config.width, config.height);
    if (!surf)
        return Status::Fail("Failed to create Raqote surface");

    // Loop 1: Create all native paths + sources
    std::vector<RqtPathBuf*> paths;
    paths.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        paths.push_back(BuildRaqotePathBuf(p));
    }
    std::vector<RqtSourceBuf*> sources;
    sources.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        sources.push_back(CreateRaqoteSource(p));
    }

    // Loop 2: Draw all
    Status s = ExecuteRaqoteCommands(scene, config, paths, sources, surf);

    // Loop 3: Destroy all paths + sources
    for (auto p : paths) {
        if (p)
            rqt_path_buf_destroy(p);
    }
    paths.clear();

    for (auto p : sources) {
        if (p)
            rqt_source_destroy(p);
    }
    sources.clear();

    rqt_get_pixels(surf, reinterpret_cast<uint32_t*>(output_buffer.data()));
    rqt_destroy(surf);
    return s;
}

void RegisterRaqoteAdapter() {
    AdapterRegistry::Instance().Register("raqote", "Raqote (Rust CPU Renderer)",
                                         []() { return std::make_unique<RaqoteAdapter>(); });
}

}  // namespace vgcpu
