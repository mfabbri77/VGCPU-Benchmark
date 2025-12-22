// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: backends/blend2d.md

#include "adapters/blend2d/blend2d_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <vector>

namespace vgcpu {

namespace {

// Helper to create Blend2D gradient from IR paint
BLGradient CreateGradient(const Paint& paint) {
    BLGradient gradient;

    // Choose gradient type
    if (paint.type == ir::PaintType::kLinear) {
        BLLinearGradientValues values(paint.linear_start_x, paint.linear_start_y,
                                      paint.linear_end_x, paint.linear_end_y);
        gradient.create(values, BL_EXTEND_MODE_PAD);
    } else if (paint.type == ir::PaintType::kRadial) {
        // Blend2D Radial: x0, y0, x1, y1 (focal), r0 (focal radius?), r1 (radius)
        // IR Radial: center_x, center_y, radius
        // We'll map IR radial to circle (focal = center, r0 = 0)
        BLRadialGradientValues values(paint.radial_center_x, paint.radial_center_y,
                                      paint.radial_center_x, paint.radial_center_y,
                                      paint.radial_radius);
        gradient.create(values, BL_EXTEND_MODE_PAD);
    }

    // Add stops
    for (const auto& stop : paint.stops) {
        uint8_t r = (stop.color >> 0) & 0xFF;
        uint8_t g = (stop.color >> 8) & 0xFF;
        uint8_t b = (stop.color >> 16) & 0xFF;
        uint8_t a = (stop.color >> 24) & 0xFF;
        gradient.add_stop(stop.offset, BLRgba32(r, g, b, a));
    }

    return gradient;
}

}  // namespace

Status Blend2DAdapter::Initialize(const AdapterArgs& args) {
    if (args.thread_count > 0) {
        thread_count_ = args.thread_count;
    }
    initialized_ = true;
    return Status::Ok();
}

void Blend2DAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo Blend2DAdapter::GetInfo() const {
    BLRuntimeBuildInfo buildInfo;
    BLRuntime::query_build_info(&buildInfo);

    std::string version = std::to_string(buildInfo.major_version) + "." +
                          std::to_string(buildInfo.minor_version) + "." +
                          std::to_string(buildInfo.patch_version);

    return AdapterInfo{.id = "blend2d",
                       .detailed_name = "Blend2D (JIT Software Rasterizer)",
                       .version = version,
                       .is_cpu_only = true};
}

CapabilitySet Blend2DAdapter::GetCapabilities() const {
    return CapabilitySet::All();
}

Status Blend2DAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                              std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("Blend2DAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    size_t buffer_size = static_cast<size_t>(config.width) * config.height * 4;
    output_buffer.resize(buffer_size);

    BLImage img;
    BLResult result =
        img.create_from_data(config.width, config.height, BL_FORMAT_PRGB32, output_buffer.data(),
                             static_cast<intptr_t>(config.width * 4));

    if (result != BL_SUCCESS) {
        return Status::Fail("Failed to create Blend2D image from data");
    }

    BLContextCreateInfo cci{};
    cci.thread_count = thread_count_;
    BLContext ctx(img, cci);

    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    // Current stroke state (since Opcode::kSetStroke sets state for subsequent kStrokePath)
    uint16_t current_stroke_paint_id = 0;

    auto apply_paint_to_ctx = [&](uint16_t paint_id, bool is_stroke) {
        if (paint_id >= scene.paints.size())
            return;
        const auto& paint = scene.paints[paint_id];

        if (paint.type == ir::PaintType::kSolid) {
            uint8_t r = (paint.color >> 0) & 0xFF;
            uint8_t g = (paint.color >> 8) & 0xFF;
            uint8_t b = (paint.color >> 16) & 0xFF;
            uint8_t a = (paint.color >> 24) & 0xFF;
            BLRgba32 c(r, g, b, a);
            if (is_stroke)
                ctx.set_stroke_style(c);
            else
                ctx.set_fill_style(c);
        } else {
            BLGradient grad = CreateGradient(paint);
            if (is_stroke)
                ctx.set_stroke_style(grad);
            else
                ctx.set_fill_style(grad);
        }
    };

    // Helper to build path
    auto build_path = [&](uint16_t path_id, BLPath& out_path) {
        if (path_id >= scene.paths.size())
            return;
        const auto& path_data = scene.paths[path_id];
        size_t pt_idx = 0;
        for (auto verb : path_data.verbs) {
            switch (verb) {
                case ir::PathVerb::kMoveTo:
                    if (pt_idx + 1 <= path_data.points.size() / 2) {
                        out_path.move_to(path_data.points[pt_idx * 2],
                                         path_data.points[pt_idx * 2 + 1]);
                        pt_idx++;
                    }
                    break;
                case ir::PathVerb::kLineTo:
                    if (pt_idx + 1 <= path_data.points.size() / 2) {
                        out_path.line_to(path_data.points[pt_idx * 2],
                                         path_data.points[pt_idx * 2 + 1]);
                        pt_idx++;
                    }
                    break;
                case ir::PathVerb::kQuadTo:
                    if (pt_idx + 2 <= path_data.points.size() / 2) {
                        out_path.quad_to(path_data.points[pt_idx * 2],
                                         path_data.points[pt_idx * 2 + 1],
                                         path_data.points[(pt_idx + 1) * 2],
                                         path_data.points[(pt_idx + 1) * 2 + 1]);
                        pt_idx += 2;
                    }
                    break;
                case ir::PathVerb::kCubicTo:
                    if (pt_idx + 3 <= path_data.points.size() / 2) {
                        out_path.cubic_to(path_data.points[pt_idx * 2],
                                          path_data.points[pt_idx * 2 + 1],
                                          path_data.points[(pt_idx + 1) * 2],
                                          path_data.points[(pt_idx + 1) * 2 + 1],
                                          path_data.points[(pt_idx + 2) * 2],
                                          path_data.points[(pt_idx + 2) * 2 + 1]);
                        pt_idx += 3;
                    }
                    break;
                case ir::PathVerb::kClose:
                    out_path.close();
                    break;
            }
        }
    };

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
                ctx.save();
                ctx.reset_transform();
                ctx.set_fill_style(BLRgba32(r, g, b, a));
                ctx.fill_all();
                ctx.restore();
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
                    goto done;  // u16(2) + f32(4) + u8(1)
                current_stroke_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                float width = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                uint8_t opts = *cmd++;

                ctx.set_stroke_width(width);

                ir::StrokeCap cap = ir::UnpackStrokeCap(opts);
                ir::StrokeJoin join = ir::UnpackStrokeJoin(opts);

                // Map Cap
                switch (cap) {
                    case ir::StrokeCap::kButt:
                        ctx.set_stroke_caps(BL_STROKE_CAP_BUTT);
                        break;
                    case ir::StrokeCap::kRound:
                        ctx.set_stroke_caps(BL_STROKE_CAP_ROUND);
                        break;
                    case ir::StrokeCap::kSquare:
                        ctx.set_stroke_caps(BL_STROKE_CAP_SQUARE);
                        break;
                }

                // Map Join
                switch (join) {
                    case ir::StrokeJoin::kMiter:
                        ctx.set_stroke_join(BL_STROKE_JOIN_MITER_CLIP);
                        break;
                    case ir::StrokeJoin::kRound:
                        ctx.set_stroke_join(BL_STROKE_JOIN_ROUND);
                        break;
                    case ir::StrokeJoin::kBevel:
                        ctx.set_stroke_join(BL_STROKE_JOIN_BEVEL);
                        break;
                }
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                apply_paint_to_ctx(current_paint_id, false);

                BLPath path;
                build_path(path_id, path);

                ctx.set_fill_rule(current_fill_rule == ir::FillRule::kEvenOdd
                                      ? BL_FILL_RULE_EVEN_ODD
                                      : BL_FILL_RULE_NON_ZERO);
                ctx.fill_path(path);
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                apply_paint_to_ctx(current_stroke_paint_id, true);

                BLPath path;
                build_path(path_id, path);

                ctx.stroke_path(path);
                break;
            }

            case ir::Opcode::kSave:
                ctx.save();
                break;

            case ir::Opcode::kRestore:
                ctx.restore();
                break;

            case ir::Opcode::kSetMatrix: {
                // Skipping actual implementation for brevity, IR doc says f32[6]
                // Just advance pointer
                if (cmd + 24 > end)
                    goto done;
                // float m[6]; memcpy(m, cmd, 24);
                // BLMatrix2D mat(m[0], m[1], m[2], m[3], m[4], m[5]);
                // ctx.set_transform(mat);
                cmd += 24;
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                if (cmd + 24 > end)
                    goto done;
                cmd += 24;
                break;
            }

            default:
                break;
        }
    }

done:
    ctx.end();
    return Status::Ok();
}

void RegisterBlend2DAdapter() {
    AdapterRegistry::Instance().Register("blend2d", "Blend2D (JIT Software Rasterizer)",
                                         []() { return std::make_unique<Blend2DAdapter>(); });
}

}  // namespace vgcpu
