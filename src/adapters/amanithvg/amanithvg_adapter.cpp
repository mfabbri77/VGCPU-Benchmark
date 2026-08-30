// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] AmanithVG backend
// (Chapter 4)

#include "adapters/amanithvg/amanithvg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

// AmanithVG SRE headers
#include <VG/openvg.h>
#include <VG/vgu.h>
// AmanithVG extensions for SRE (headless rendering)
#define VG_VGEXT_PROTOTYPES
#include <VG/vgext.h>

#include <cstring>
#include <vector>

namespace vgcpu {

namespace {

// Convert IR color (RGBA) to OpenVG paint color.
// R<->B swap: the AmanithVG SRE surface created by
// vgPrivSurfaceCreateByPointerMZT stores bytes B,G,R,A on little-endian;
// the adapter contract wants R,G,B,A. Swapped input colors make the
// rendered bytes land in contract order at zero per-pixel cost (blending,
// coverage and gradient interpolation treat channels symmetrically).
void SetPaintColor(VGPaint paint, uint32_t rgba) {
    VGfloat color[4];
    color[0] = ((rgba >> 16) & 0xFF) / 255.0f;  // B fed as R
    color[1] = ((rgba >> 8) & 0xFF) / 255.0f;   // G
    color[2] = ((rgba >> 0) & 0xFF) / 255.0f;   // R fed as B
    color[3] = ((rgba >> 24) & 0xFF) / 255.0f;  // A
    vgSetParameterfv(paint, VG_PAINT_COLOR, 4, color);
}

// Create an OpenVG path from IR path data
VGPath CreatePath(const Path& path_data) {
    VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0,
                               VG_PATH_CAPABILITY_ALL);

    if (path == VG_INVALID_HANDLE)
        return VG_INVALID_HANDLE;

    // Build path commands
    std::vector<VGubyte> cmds;
    std::vector<VGfloat> coords;

    size_t pt_idx = 0;
    for (auto verb : path_data.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                cmds.push_back(VG_MOVE_TO_ABS);
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    coords.push_back(path_data.points[pt_idx * 2]);
                    coords.push_back(path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kLineTo:
                cmds.push_back(VG_LINE_TO_ABS);
                if (pt_idx * 2 + 1 < path_data.points.size()) {
                    coords.push_back(path_data.points[pt_idx * 2]);
                    coords.push_back(path_data.points[pt_idx * 2 + 1]);
                }
                pt_idx++;
                break;
            case ir::PathVerb::kQuadTo:
                cmds.push_back(VG_QUAD_TO_ABS);
                if ((pt_idx + 1) * 2 + 1 < path_data.points.size()) {
                    // Control point
                    coords.push_back(path_data.points[pt_idx * 2]);
                    coords.push_back(path_data.points[pt_idx * 2 + 1]);
                    // End point
                    coords.push_back(path_data.points[(pt_idx + 1) * 2]);
                    coords.push_back(path_data.points[(pt_idx + 1) * 2 + 1]);
                }
                pt_idx += 2;
                break;
            case ir::PathVerb::kCubicTo:
                cmds.push_back(VG_CUBIC_TO_ABS);
                if ((pt_idx + 2) * 2 + 1 < path_data.points.size()) {
                    // Control point 1
                    coords.push_back(path_data.points[pt_idx * 2]);
                    coords.push_back(path_data.points[pt_idx * 2 + 1]);
                    // Control point 2
                    coords.push_back(path_data.points[(pt_idx + 1) * 2]);
                    coords.push_back(path_data.points[(pt_idx + 1) * 2 + 1]);
                    // End point
                    coords.push_back(path_data.points[(pt_idx + 2) * 2]);
                    coords.push_back(path_data.points[(pt_idx + 2) * 2 + 1]);
                }
                pt_idx += 3;
                break;
            case ir::PathVerb::kClose:
                cmds.push_back(VG_CLOSE_PATH);
                break;
        }
    }

    if (!cmds.empty()) {
        vgAppendPathData(path, static_cast<VGint>(cmds.size()), cmds.data(), coords.data());
    }

    return path;
}

// Apply gradient paint
void ApplyGradientPaint(VGPaint paint, const Paint& ir_paint) {
    if (ir_paint.type == ir::PaintType::kLinear) {
        vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
        VGfloat gradient[4] = {ir_paint.linear_start_x, ir_paint.linear_start_y,
                               ir_paint.linear_end_x, ir_paint.linear_end_y};
        vgSetParameterfv(paint, VG_PAINT_LINEAR_GRADIENT, 4, gradient);
    } else if (ir_paint.type == ir::PaintType::kRadial) {
        vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
        VGfloat gradient[5] = {ir_paint.radial_center_x, ir_paint.radial_center_y,
                               ir_paint.radial_center_x, ir_paint.radial_center_y,
                               ir_paint.radial_radius};
        vgSetParameterfv(paint, VG_PAINT_RADIAL_GRADIENT, 5, gradient);
    }

    // Set color ramp stops
    if (!ir_paint.stops.empty()) {
        std::vector<VGfloat> stops;
        stops.reserve(ir_paint.stops.size() * 5);  // offset + RGBA for each stop
        for (const auto& s : ir_paint.stops) {
            stops.push_back(s.offset);
            stops.push_back(((s.color >> 16) & 0xFF) / 255.0f);  // B fed as R (see SetPaintColor)
            stops.push_back(((s.color >> 8) & 0xFF) / 255.0f);   // G
            stops.push_back(((s.color >> 0) & 0xFF) / 255.0f);   // R fed as B
            stops.push_back(((s.color >> 24) & 0xFF) / 255.0f);  // A
        }
        vgSetParameterfv(paint, VG_PAINT_COLOR_RAMP_STOPS, static_cast<VGint>(stops.size()),
                         stops.data());
    }
}

}  // namespace

Status AmanithVGAdapter::Initialize(const AdapterArgs& /*args*/) {
    // Initialize AmanithVG library
    if (vgInitializeMZT() != VG_TRUE) {
        return Status::Fail("Failed to initialize AmanithVG library");
    }
    context_ = vgPrivContextCreateMZT(nullptr);
    if (!context_) {
        vgTerminateMZT();
        return Status::Fail("Failed to create AmanithVG context");
    }
    initialized_ = true;
    return Status::Ok();
}

Status AmanithVGAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_ || !context_) {
        return Status::Fail("AmanithVGAdapter not initialized");
    }

    // Bind context to a minimal offscreen surface during Prepare so we can
    // create and populate OpenVG path objects.
    void* dummy_surface = vgPrivSurfaceCreateMZT(16, 16, VG_FALSE, VG_TRUE, VG_FALSE);
    if (!dummy_surface) {
        return Status::Fail("Failed to create temporary AmanithVG surface");
    }
    vgPrivMakeCurrentMZT(context_, dummy_surface);

    DestroyPaths();

    // Pre-create all VGPath handles for the scene (proper OpenVG architecture)
    vg_paths_.reserve(scene.paths.size());
    for (const auto& ir_path : scene.paths) {
        vg_paths_.push_back(CreatePath(ir_path));
    }

    vgPrivMakeCurrentMZT(nullptr, nullptr);
    vgPrivSurfaceDestroyMZT(dummy_surface);
    return Status::Ok();
}

void AmanithVGAdapter::DestroyPaths() {
    for (auto p : vg_paths_) {
        if (p != VG_INVALID_HANDLE) {
            vgDestroyPath(p);
        }
    }
    vg_paths_.clear();
}

void AmanithVGAdapter::Shutdown() {
    if (initialized_) {
        if (context_) {
            void* dummy_surface = vgPrivSurfaceCreateMZT(16, 16, VG_FALSE, VG_TRUE, VG_FALSE);
            if (dummy_surface) {
                vgPrivMakeCurrentMZT(context_, dummy_surface);
                DestroyPaths();
                vgPrivMakeCurrentMZT(nullptr, nullptr);
                vgPrivSurfaceDestroyMZT(dummy_surface);
            }
            vgPrivContextDestroyMZT(context_);
            context_ = nullptr;
        }
        vgTerminateMZT();
        initialized_ = false;
    }
}

AdapterInfo AmanithVGAdapter::GetInfo() const {
    return AdapterInfo{.id = "amanithvg",
                       .detailed_name = "AmanithVG SRE (Software Rendering Engine)",
                       .version = "6.0.0",
                       .is_cpu_only = true};
}

CapabilitySet AmanithVGAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

namespace {

Status ExecuteAmanithVGCommands(const PreparedScene& scene, const SurfaceConfig& config,
                                const std::vector<uint32_t>& paths, VGPaint fill_paint,
                                VGPaint stroke_paint, const VGfloat flip_matrix[9]) {
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

                VGfloat color[4];
                color[0] = ((rgba >> 16) & 0xFF) / 255.0f;  // B fed as R
                color[1] = ((rgba >> 8) & 0xFF) / 255.0f;
                color[2] = ((rgba >> 0) & 0xFF) / 255.0f;  // R fed as B
                color[3] = ((rgba >> 24) & 0xFF) / 255.0f;
                vgSetfv(VG_CLEAR_COLOR, 4, color);
                vgClear(0, 0, config.width, config.height);
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

                const auto& ir_paint = scene.paints[current_paint_id];
                VGPath path = paths[path_id];
                if (path == VG_INVALID_HANDLE)
                    break;

                if (ir_paint.type == ir::PaintType::kSolid) {
                    vgSetParameteri(fill_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
                    SetPaintColor(fill_paint, ir_paint.color);
                } else {
                    ApplyGradientPaint(fill_paint, ir_paint);
                }
                vgSetPaint(fill_paint, VG_FILL_PATH);

                vgSeti(VG_FILL_RULE,
                       current_fill_rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);

                vgDrawPath(path, VG_FILL_PATH);
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id >= paths.size() || current_stroke_paint_id >= scene.paints.size())
                    break;

                const auto& ir_paint = scene.paints[current_stroke_paint_id];
                VGPath path = paths[path_id];
                if (path == VG_INVALID_HANDLE)
                    break;

                if (ir_paint.type == ir::PaintType::kSolid) {
                    vgSetParameteri(stroke_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
                    SetPaintColor(stroke_paint, ir_paint.color);
                } else {
                    ApplyGradientPaint(stroke_paint, ir_paint);
                }
                vgSetPaint(stroke_paint, VG_STROKE_PATH);

                vgSetf(VG_STROKE_LINE_WIDTH, current_stroke_width);

                VGCapStyle cap = VG_CAP_BUTT;
                switch (current_stroke_cap) {
                    case ir::StrokeCap::kButt:
                        cap = VG_CAP_BUTT;
                        break;
                    case ir::StrokeCap::kRound:
                        cap = VG_CAP_ROUND;
                        break;
                    case ir::StrokeCap::kSquare:
                        cap = VG_CAP_SQUARE;
                        break;
                }
                vgSeti(VG_STROKE_CAP_STYLE, cap);

                VGJoinStyle join = VG_JOIN_MITER;
                switch (current_stroke_join) {
                    case ir::StrokeJoin::kMiter:
                        join = VG_JOIN_MITER;
                        break;
                    case ir::StrokeJoin::kRound:
                        join = VG_JOIN_ROUND;
                        break;
                    case ir::StrokeJoin::kBevel:
                        join = VG_JOIN_BEVEL;
                        break;
                }
                vgSeti(VG_STROKE_JOIN_STYLE, join);

                vgDrawPath(path, VG_STROKE_PATH);
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kSetMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;

                VGfloat matrix[9] = {m[0], m[2], m[4],
                                     m[1], m[3], m[5],
                                     0.0f, 0.0f, 1.0f};
                vgLoadMatrix(flip_matrix);
                vgMultMatrix(matrix);
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;

                VGfloat matrix[9] = {m[0], m[2], m[4], m[1], m[3], m[5], 0.0f, 0.0f, 1.0f};
                vgMultMatrix(matrix);
                break;
            }

            case ir::Opcode::kSetDash: {
                if (cmd + 5 > end)
                    goto done;
                uint8_t count = *cmd++;
                float phase = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                if (cmd + 4 * count > end)
                    goto done;
                const float* lengths = reinterpret_cast<const float*>(cmd);
                cmd += 4 * count;
                if (count > 0) {
                    vgSetfv(VG_STROKE_DASH_PATTERN, count, lengths);
                    vgSetf(VG_STROKE_DASH_PHASE, phase);
                } else {
                    vgSetfv(VG_STROKE_DASH_PATTERN, 0, nullptr);
                }
                break;
            }

            case ir::Opcode::kClipPush: {
                if (cmd + 3 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                ir::FillRule rule = static_cast<ir::FillRule>(*cmd++);

                if (path_id < paths.size()) {
                    VGPath path = paths[path_id];
                    if (path != VG_INVALID_HANDLE) {
                        vgSeti(static_cast<VGParamType>(VG_CLIP_RULE_MZT),
                               rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);
                        vgSeti(VG_MATRIX_MODE,
                               static_cast<VGMatrixMode>(VG_MATRIX_CLIP_USER_TO_SURFACE_MZT));
                        vgLoadMatrix(flip_matrix);
                        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
                        vgClipPathPushMZT(path, VG_TRUE);
                    }
                }
                break;
            }

            case ir::Opcode::kClipPop:
                vgClipPathPopMZT();
                break;

            default:
                break;
        }
    }

done:
    return Status::Ok();
}

}  // namespace

Status AmanithVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                                std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("AmanithVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    void* surface = vgPrivSurfaceCreateByPointerMZT(config.width, config.height,
                                                    VG_FALSE, VG_TRUE,
                                                    output_buffer.data(), nullptr);
    if (!surface) {
        return Status::Fail("Failed to create AmanithVG surface");
    }
    if (vgPrivMakeCurrentMZT(context_, surface) != VG_TRUE) {
        vgPrivSurfaceDestroyMZT(surface);
        return Status::Fail("Failed to bind AmanithVG context and surface");
    }

    vgSeti(VG_RENDERING_QUALITY, VG_RENDERING_QUALITY_BETTER);
    vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
    vgSetfv(VG_STROKE_DASH_PATTERN, 0, nullptr);
    vgSetf(VG_STROKE_DASH_PHASE, 0.0f);
    vgClipPathClearMZT();

    const VGfloat flip_matrix[9] = {1.0f, 0.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f,
                                    0.0f, static_cast<VGfloat>(config.height), 1.0f};
    vgLoadMatrix(flip_matrix);

    VGPaint fill_paint = vgCreatePaint();
    VGPaint stroke_paint = vgCreatePaint();

    Status s = ExecuteAmanithVGCommands(scene, config, vg_paths_, fill_paint, stroke_paint, flip_matrix);

    vgDestroyPaint(fill_paint);
    vgDestroyPaint(stroke_paint);
    vgFinish();

    vgPrivMakeCurrentMZT(nullptr, nullptr);
    vgPrivSurfaceDestroyMZT(surface);
    return s;
}

Status AmanithVGAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                         std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("AmanithVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    void* surface = vgPrivSurfaceCreateByPointerMZT(config.width, config.height,
                                                    VG_FALSE, VG_TRUE,
                                                    output_buffer.data(), nullptr);
    if (!surface) {
        return Status::Fail("Failed to create AmanithVG surface");
    }
    if (vgPrivMakeCurrentMZT(context_, surface) != VG_TRUE) {
        vgPrivSurfaceDestroyMZT(surface);
        return Status::Fail("Failed to bind AmanithVG context and surface");
    }

    vgSeti(VG_RENDERING_QUALITY, VG_RENDERING_QUALITY_BETTER);
    vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
    vgSetfv(VG_STROKE_DASH_PATTERN, 0, nullptr);
    vgSetf(VG_STROKE_DASH_PHASE, 0.0f);
    vgClipPathClearMZT();

    const VGfloat flip_matrix[9] = {1.0f, 0.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f,
                                    0.0f, static_cast<VGfloat>(config.height), 1.0f};
    vgLoadMatrix(flip_matrix);

    VGPaint fill_paint = vgCreatePaint();
    VGPaint stroke_paint = vgCreatePaint();

    // Loop 1: Create all native paths
    std::vector<uint32_t> paths;
    paths.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        paths.push_back(CreatePath(p));
    }

    // Loop 2: Draw all
    Status s = ExecuteAmanithVGCommands(scene, config, paths, fill_paint, stroke_paint, flip_matrix);

    // Loop 3: Destroy all
    for (auto p : paths) {
        if (p != VG_INVALID_HANDLE) {
            vgDestroyPath(p);
        }
    }
    paths.clear();

    vgDestroyPaint(fill_paint);
    vgDestroyPaint(stroke_paint);
    vgFinish();

    vgPrivMakeCurrentMZT(nullptr, nullptr);
    vgPrivSurfaceDestroyMZT(surface);
    return s;
}

void RegisterAmanithVGAdapter() {
    AdapterRegistry::Instance().Register("amanithvg", "AmanithVG SRE (Software Rendering Engine)",
                                         []() { return std::make_unique<AmanithVGAdapter>(); });
}

}  // namespace vgcpu
