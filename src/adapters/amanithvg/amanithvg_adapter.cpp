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
    if (path_data.verbs.empty()) {
        return vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0,
                            VG_PATH_CAPABILITY_ALL);
    }

    VGPath path =
        vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f,
                     static_cast<VGint>(path_data.verbs.size()),
                     static_cast<VGint>(path_data.points.size() / 2), VG_PATH_CAPABILITY_ALL);

    if (path == VG_INVALID_HANDLE)
        return VG_INVALID_HANDLE;

    // Fast verb-to-command translation using thread-local buffer to avoid heap allocations
    thread_local std::vector<VGubyte> cmds;
    cmds.resize(path_data.verbs.size());

    for (size_t i = 0; i < path_data.verbs.size(); ++i) {
        switch (path_data.verbs[i]) {
            case ir::PathVerb::kMoveTo:
                cmds[i] = VG_MOVE_TO_ABS;
                break;
            case ir::PathVerb::kLineTo:
                cmds[i] = VG_LINE_TO_ABS;
                break;
            case ir::PathVerb::kQuadTo:
                cmds[i] = VG_QUAD_TO_ABS;
                break;
            case ir::PathVerb::kCubicTo:
                cmds[i] = VG_CUBIC_TO_ABS;
                break;
            case ir::PathVerb::kClose:
                cmds[i] = VG_CLOSE_PATH;
                break;
            default:
                cmds[i] = VG_CLOSE_PATH;
                break;
        }
    }

    vgAppendPathData(path, static_cast<VGint>(cmds.size()), cmds.data(), path_data.points.data());
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

VGPaint CreateOpenVGPaint(const Paint& ir_paint) {
    VGPaint pt = vgCreatePaint();
    if (pt == VG_INVALID_HANDLE)
        return pt;
    if (ir_paint.type == ir::PaintType::kSolid) {
        vgSetParameteri(pt, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
        SetPaintColor(pt, ir_paint.color);
    } else {
        ApplyGradientPaint(pt, ir_paint);
    }
    return pt;
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
    // create and populate OpenVG path and paint objects.
    void* dummy_surface = vgPrivSurfaceCreateMZT(16, 16, VG_FALSE, VG_TRUE, VG_FALSE);
    if (!dummy_surface) {
        return Status::Fail("Failed to create temporary AmanithVG surface");
    }
    vgPrivMakeCurrentMZT(context_, dummy_surface);

    DestroyPaths();
    DestroyPaints();

    // Pre-create all VGPath handles for the scene (proper OpenVG architecture)
    vg_paths_.reserve(scene.paths.size());
    for (const auto& ir_path : scene.paths) {
        vg_paths_.push_back(CreatePath(ir_path));
    }

    // Pre-create all VGPaint handles for the scene
    vg_paints_.reserve(scene.paints.size());
    for (const auto& ir_paint : scene.paints) {
        vg_paints_.push_back(CreateOpenVGPaint(ir_paint));
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

void AmanithVGAdapter::DestroyPaints() {
    for (auto p : vg_paints_) {
        if (p != VG_INVALID_HANDLE) {
            vgDestroyPaint(p);
        }
    }
    vg_paints_.clear();
}

void AmanithVGAdapter::Shutdown() {
    if (initialized_) {
        if (context_) {
            void* dummy_surface = vgPrivSurfaceCreateMZT(16, 16, VG_FALSE, VG_TRUE, VG_FALSE);
            if (dummy_surface) {
                vgPrivMakeCurrentMZT(context_, dummy_surface);
                DestroyPaths();
                DestroyPaints();
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
                                const std::vector<uint32_t>& paths,
                                const std::vector<uint32_t>& paints, const VGfloat flip_matrix[9]) {
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

                if (current_paint_id < paints.size() &&
                    paints[current_paint_id] != VG_INVALID_HANDLE) {
                    vgSetPaint(paints[current_paint_id], VG_FILL_PATH);
                }
                vgSeti(VG_FILL_RULE,
                       current_fill_rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);
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
                    paints[current_stroke_paint_id] != VG_INVALID_HANDLE) {
                    vgSetPaint(paints[current_stroke_paint_id], VG_STROKE_PATH);
                }
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
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < paths.size() && paths[path_id] != VG_INVALID_HANDLE) {
                    vgDrawPath(paths[path_id], VG_FILL_PATH);
                }
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < paths.size() && paths[path_id] != VG_INVALID_HANDLE) {
                    vgDrawPath(paths[path_id], VG_STROKE_PATH);
                }
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

                VGfloat matrix[9] = {m[0], m[2], m[4], m[1], m[3], m[5], 0.0f, 0.0f, 1.0f};
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

    void* surface = vgPrivSurfaceCreateByPointerMZT(config.width, config.height, VG_FALSE, VG_TRUE,
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

    const VGfloat flip_matrix[9] = {
        1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, static_cast<VGfloat>(config.height), 1.0f};
    vgLoadMatrix(flip_matrix);

    Status s = ExecuteAmanithVGCommands(scene, config, vg_paths_, vg_paints_, flip_matrix);
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

    void* surface = vgPrivSurfaceCreateByPointerMZT(config.width, config.height, VG_FALSE, VG_TRUE,
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

    const VGfloat flip_matrix[9] = {
        1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, static_cast<VGfloat>(config.height), 1.0f};
    vgLoadMatrix(flip_matrix);

    // Immediate 1-loop execution: CreateOpenVGPaint for paints, and for each drawcall
    // create path -> draw -> destroy path immediately to test working-set cache behavior.
    std::vector<uint32_t> paints;
    paints.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        paints.push_back(CreateOpenVGPaint(p));
    }

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

                if (current_paint_id < paints.size() &&
                    paints[current_paint_id] != VG_INVALID_HANDLE) {
                    vgSetPaint(paints[current_paint_id], VG_FILL_PATH);
                }
                vgSeti(VG_FILL_RULE,
                       current_fill_rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);
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
                    paints[current_stroke_paint_id] != VG_INVALID_HANDLE) {
                    vgSetPaint(paints[current_stroke_paint_id], VG_STROKE_PATH);
                }
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
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < scene.paths.size()) {
                    VGPath path = CreatePath(scene.paths[path_id]);
                    if (path != VG_INVALID_HANDLE) {
                        vgDrawPath(path, VG_FILL_PATH);
                        vgDestroyPath(path);
                    }
                }
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < scene.paths.size()) {
                    VGPath path = CreatePath(scene.paths[path_id]);
                    if (path != VG_INVALID_HANDLE) {
                        vgDrawPath(path, VG_STROKE_PATH);
                        vgDestroyPath(path);
                    }
                }
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

                VGfloat matrix[9] = {m[0], m[2], m[4], m[1], m[3], m[5], 0.0f, 0.0f, 1.0f};
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

                if (path_id < scene.paths.size()) {
                    VGPath path = CreatePath(scene.paths[path_id]);
                    if (path != VG_INVALID_HANDLE) {
                        vgSeti(static_cast<VGParamType>(VG_CLIP_RULE_MZT),
                               rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);
                        vgSeti(VG_MATRIX_MODE,
                               static_cast<VGMatrixMode>(VG_MATRIX_CLIP_USER_TO_SURFACE_MZT));
                        vgLoadMatrix(flip_matrix);
                        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
                        vgClipPathPushMZT(path, VG_TRUE);
                        vgDestroyPath(path);
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
    vgFinish();

    for (auto p : paints) {
        if (p != VG_INVALID_HANDLE) {
            vgDestroyPaint(p);
        }
    }
    paints.clear();

    vgPrivMakeCurrentMZT(nullptr, nullptr);
    vgPrivSurfaceDestroyMZT(surface);
    return Status::Ok();
}

void RegisterAmanithVGAdapter() {
    AdapterRegistry::Instance().Register("amanithvg", "AmanithVG SRE (Software Rendering Engine)",
                                         []() { return std::make_unique<AmanithVGAdapter>(); });
}

}  // namespace vgcpu
