// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: backends/amanithvg.md

#include "adapters/amanithvg/amanithvg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

// AmanithVG SRE headers
#include <VG/openvg.h>
#include <VG/vgu.h>
// AmanithVG extensions for SRE (headless rendering)
#include <VG/vgext.h>

#include <cstring>
#include <vector>

namespace vgcpu {

namespace {

// Convert IR color (RGBA) to OpenVG color (RGBA as floats)
void SetPaintColor(VGPaint paint, uint32_t rgba) {
    VGfloat color[4];
    color[0] = ((rgba >> 0) & 0xFF) / 255.0f;   // R
    color[1] = ((rgba >> 8) & 0xFF) / 255.0f;   // G
    color[2] = ((rgba >> 16) & 0xFF) / 255.0f;  // B
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
            stops.push_back(((s.color >> 0) & 0xFF) / 255.0f);   // R
            stops.push_back(((s.color >> 8) & 0xFF) / 255.0f);   // G
            stops.push_back(((s.color >> 16) & 0xFF) / 255.0f);  // B
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
    initialized_ = true;
    return Status::Ok();
}

void AmanithVGAdapter::Shutdown() {
    if (initialized_) {
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

Status AmanithVGAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                                std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("AmanithVGAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    // Create OpenVG context (no shared context)
    void* context = vgPrivContextCreateMZT(nullptr);
    if (!context) {
        return Status::Fail("Failed to create AmanithVG context");
    }

    // Create surface using direct pixel buffer
    // Parameters: width, height, linearColorSpace, alphaPremultiplied, pixels, alphaMaskPixels
    void* surface = vgPrivSurfaceCreateByPointerMZT(config.width, config.height,
                                                    VG_FALSE,  // sRGB (non-linear) color space
                                                    VG_TRUE,   // alpha premultiplied
                                                    output_buffer.data(),
                                                    nullptr);  // no alpha mask

    if (!surface) {
        vgPrivContextDestroyMZT(context);
        return Status::Fail("Failed to create AmanithVG surface");
    }

    // Bind context and surface
    if (vgPrivMakeCurrentMZT(context, surface) != VG_TRUE) {
        vgPrivSurfaceDestroyMZT(surface);
        vgPrivContextDestroyMZT(context);
        return Status::Fail("Failed to bind AmanithVG context and surface");
    }

    // Set default rendering state
    vgSeti(VG_RENDERING_QUALITY, VG_RENDERING_QUALITY_BETTER);
    vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
    vgLoadIdentity();

    // Create reusable paint handles
    VGPaint fill_paint = vgCreatePaint();
    VGPaint stroke_paint = vgCreatePaint();

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

                VGfloat color[4];
                color[0] = ((rgba >> 0) & 0xFF) / 255.0f;
                color[1] = ((rgba >> 8) & 0xFF) / 255.0f;
                color[2] = ((rgba >> 16) & 0xFF) / 255.0f;
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

                if (path_id >= scene.paths.size())
                    break;
                if (current_paint_id >= scene.paints.size())
                    break;

                const auto& ir_paint = scene.paints[current_paint_id];
                VGPath path = CreatePath(scene.paths[path_id]);
                if (path == VG_INVALID_HANDLE)
                    break;

                // Configure paint
                if (ir_paint.type == ir::PaintType::kSolid) {
                    vgSetParameteri(fill_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
                    SetPaintColor(fill_paint, ir_paint.color);
                } else {
                    ApplyGradientPaint(fill_paint, ir_paint);
                }
                vgSetPaint(fill_paint, VG_FILL_PATH);

                // Set fill rule
                vgSeti(VG_FILL_RULE,
                       current_fill_rule == ir::FillRule::kEvenOdd ? VG_EVEN_ODD : VG_NON_ZERO);

                vgDrawPath(path, VG_FILL_PATH);
                vgDestroyPath(path);
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

                const auto& ir_paint = scene.paints[current_stroke_paint_id];
                VGPath path = CreatePath(scene.paths[path_id]);
                if (path == VG_INVALID_HANDLE)
                    break;

                // Configure stroke paint
                if (ir_paint.type == ir::PaintType::kSolid) {
                    vgSetParameteri(stroke_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
                    SetPaintColor(stroke_paint, ir_paint.color);
                } else {
                    ApplyGradientPaint(stroke_paint, ir_paint);
                }
                vgSetPaint(stroke_paint, VG_STROKE_PATH);

                // Configure stroke parameters
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
                vgDestroyPath(path);
                break;
            }

            case ir::Opcode::kSave:
                // OpenVG doesn't have save/restore for full state
                // but we can push matrix
                break;

            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kSetMatrix: {
                if (cmd + 24 > end)
                    goto done;
                // OpenVG uses 3x3 affine matrix (column-major)
                // IR uses [a b c d e f] = [m00 m01 m10 m11 m02 m12]
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;

                VGfloat matrix[9] = {m[0], m[2], m[4],   // column 0
                                     m[1], m[3], m[5],   // column 1
                                     0.0f, 0.0f, 1.0f};  // column 2
                vgLoadMatrix(matrix);
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

            default:
                break;
        }
    }

done:
    // Cleanup OpenVG objects
    vgDestroyPaint(fill_paint);
    vgDestroyPaint(stroke_paint);

    // Flush rendering (SRE should be synchronous, but ensure completion)
    vgFinish();

    // Release context and surface
    vgPrivMakeCurrentMZT(nullptr, nullptr);
    vgPrivSurfaceDestroyMZT(surface);
    vgPrivContextDestroyMZT(context);

    return Status::Ok();
}

void RegisterAmanithVGAdapter() {
    AdapterRegistry::Instance().Register("amanithvg", "AmanithVG SRE (Software Rendering Engine)",
                                         []() { return std::make_unique<AmanithVGAdapter>(); });
}

}  // namespace vgcpu
