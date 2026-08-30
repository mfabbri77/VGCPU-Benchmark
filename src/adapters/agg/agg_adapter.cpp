/*
 * Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
 * SPDX-License-Identifier: MIT
 */
#include "adapters/agg/agg_adapter.h"

// AGG includes
#include "adapters/adapter_registry.h"
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 5054 5055)
#endif

#include "agg_conv_curve.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_gradient_lut.h"
#include "agg_path_storage.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_span_allocator.h"
#include "agg_span_gradient.h"
#include "agg_span_interpolator_linear.h"
#include "agg_trans_affine.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "ir/prepared_scene.h"

#include <cmath>
#include <cstring>
#include <optional>

namespace vgcpu::adapters::agg_backend {

namespace {
template <typename T>
T ReadLE(const uint8_t*& ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    ptr += sizeof(T);
    return val;
}

// Render the already-rasterized path with a gradient span generator.
// AGG has no "set gradient" call: gradients are span generators wired into
// render_scanlines_aa (LUT of 256 interpolated stop colors + a device->
// gradient-space affine). No R<->B swap here: pixfmt_rgba32 is true RGBA.
template <typename Rasterizer, typename Scanline, typename RenBase>
void RenderGradientFill(Rasterizer& ras, Scanline& sl, RenBase& ren_base,
                        const vgcpu::Paint& paint) {
    using LutType = agg::gradient_lut<agg::color_interpolator<agg::rgba8>, 256>;
    LutType lut;
    for (const auto& s : paint.stops) {
        lut.add_color(s.offset, agg::rgba8(s.color & 0xFF, (s.color >> 8) & 0xFF,
                                           (s.color >> 16) & 0xFF, (s.color >> 24) & 0xFF));
    }
    lut.build_lut();
    agg::span_allocator<agg::rgba8> alloc;

    if (paint.type == ir::PaintType::kLinear) {
        double dx = static_cast<double>(paint.linear_end_x) - paint.linear_start_x;
        double dy = static_cast<double>(paint.linear_end_y) - paint.linear_start_y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) {
            len = 1e-6;
        }
        agg::trans_affine mtx;  // device -> gradient space (inverse of rotate+translate)
        mtx *= agg::trans_affine_rotation(std::atan2(dy, dx));
        mtx *= agg::trans_affine_translation(paint.linear_start_x, paint.linear_start_y);
        mtx.invert();
        agg::span_interpolator_linear<> inter(mtx);
        agg::gradient_x gfunc;
        agg::span_gradient<agg::rgba8, agg::span_interpolator_linear<>, agg::gradient_x, LutType>
            sg(inter, gfunc, lut, 0.0, len);
        agg::render_scanlines_aa(ras, sl, ren_base, alloc, sg);
    } else {
        double radius = paint.radial_radius > 1e-6f ? paint.radial_radius : 1e-6;
        agg::trans_affine mtx;
        mtx *= agg::trans_affine_translation(paint.radial_center_x, paint.radial_center_y);
        mtx.invert();
        agg::span_interpolator_linear<> inter(mtx);
        agg::gradient_radial_d gfunc;
        agg::span_gradient<agg::rgba8, agg::span_interpolator_linear<>, agg::gradient_radial_d,
                           LutType>
            sg(inter, gfunc, lut, 0.0, radius);
        agg::render_scanlines_aa(ras, sl, ren_base, alloc, sg);
    }
}
}  // namespace

AggAdapter::AggAdapter() = default;
AggAdapter::~AggAdapter() = default;

Status AggAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    initialized_ = true;
    return Status::Ok();
}

Status AggAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("AggAdapter not initialized");
    }
    return Status::Ok();
}

void AggAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo AggAdapter::GetInfo() const {
    return AdapterInfo{.id = "agg",
                       .detailed_name = "Anti-Grain Geometry 2.6",
                       .version = "2.6",
                       .is_cpu_only = true};
}

CapabilitySet AggAdapter::GetCapabilities() const {
    return CapabilitySet::All();  // AGG supports most things
}

Status AggAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                          std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("Not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");

    uint32_t width = config.width;
    uint32_t height = config.height;
    uint32_t stride = width * 4;

    // Resize buffer
    if (output_buffer.size() != stride * height) {
        output_buffer.resize(stride * height);
    }

    // 1. Setup AGG Rendering Pipeline
    agg::rendering_buffer rbuf(output_buffer.data(), width, height, stride);

    // Pixel format: AGG's rgba32 order.
    // Assuming RGBA8888 (R=0, G=1, B=2, A=3).
    // AGG pixfmt_rgba32 usually expects R-G-B-A byte order in memory.
    using pixfmt_t = agg::pixfmt_rgba32;
    pixfmt_t pixf(rbuf);

    using ren_base_t = agg::renderer_base<pixfmt_t>;
    ren_base_t ren_base(pixf);

    // Rasterizer
    agg::rasterizer_scanline_aa<> ras;
    agg::scanline_p8 sl;

    // State
    agg::trans_affine ctm;

    // Commands
    const uint8_t* ptr = scene.command_stream.data();
    const uint8_t* end = ptr + scene.command_stream.size();

    uint16_t current_paint_id = 0xFFFF;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    float current_stroke_width = 1.0f;
    uint8_t current_stroke_opts = 0;

    while (ptr < end) {
        ir::Opcode op = static_cast<ir::Opcode>(*ptr++);

        switch (op) {
            case ir::Opcode::kEnd:
                return Status::Ok();

            case ir::Opcode::kClear: {
                uint32_t c = ReadLE<uint32_t>(ptr);
                // c is 0xAABBGGRR (little endian read of RGBA bytes) if stream was written as u32
                // Or stream has R G B A bytes.
                // IR Loader example: 0xFF, 0xFF, 0xFF, 0xFF.
                // Let's assume input color is packed RGBA8.
                // AGG color needs decomposiiton.
                uint8_t r = c & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = (c >> 16) & 0xFF;
                uint8_t a = (c >> 24) & 0xFF;
                ren_base.clear(agg::rgba8(r, g, b, a));
                break;
            }

            case ir::Opcode::kSetMatrix: {
                // m:f32[6]
                float m[6];
                std::memcpy(m, ptr, 24);
                ptr += 24;
                ctm = agg::trans_affine(m[0], m[1], m[2], m[3], m[4], m[5]);
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                float m[6];
                std::memcpy(m, ptr, 24);
                ptr += 24;
                agg::trans_affine next(m[0], m[1], m[2], m[3], m[4], m[5]);
                ctm.multiply(next);
                break;
            }

            case ir::Opcode::kSetFill: {
                current_paint_id = ReadLE<uint16_t>(ptr);
                current_fill_rule = static_cast<ir::FillRule>(*ptr++);
                break;
            }

            case ir::Opcode::kSetStroke: {
                current_paint_id = ReadLE<uint16_t>(ptr);
                current_stroke_width = ReadLE<float>(ptr);
                current_stroke_opts = *ptr++;
                (void)current_stroke_opts;  // Suppress unused warning
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                // TODO: Implement state stack
                break;

            case ir::Opcode::kFillPath:
            case ir::Opcode::kStrokePath: {
                uint16_t path_id = ReadLE<uint16_t>(ptr);
                if (path_id >= scene.paths.size())
                    break;

                const auto& ir_path = scene.paths[path_id];

                // Reconstruct path
                agg::path_storage p;
                size_t pt_idx = 0;
                for (auto verb : ir_path.verbs) {
                    switch (verb) {
                        case ir::PathVerb::kMoveTo:
                            p.move_to(ir_path.points[pt_idx], ir_path.points[pt_idx + 1]);
                            pt_idx += 2;
                            break;
                        case ir::PathVerb::kLineTo:
                            p.line_to(ir_path.points[pt_idx], ir_path.points[pt_idx + 1]);
                            pt_idx += 2;
                            break;
                        case ir::PathVerb::kQuadTo:
                            // AGG curve3
                            p.curve3(ir_path.points[pt_idx], ir_path.points[pt_idx + 1],
                                     ir_path.points[pt_idx + 2], ir_path.points[pt_idx + 3]);
                            pt_idx += 4;
                            break;
                        case ir::PathVerb::kCubicTo:
                            p.curve4(ir_path.points[pt_idx], ir_path.points[pt_idx + 1],
                                     ir_path.points[pt_idx + 2], ir_path.points[pt_idx + 3],
                                     ir_path.points[pt_idx + 4], ir_path.points[pt_idx + 5]);
                            pt_idx += 6;
                            break;
                        case ir::PathVerb::kClose:
                            p.close_polygon();
                            break;
                    }
                }

                // Flatten curves, then transform. Fixed 2026-08-30: feeding
                // path_storage straight into the rasterizer treats curve3/
                // curve4 verbs as polylines through their control points
                // (circles rendered as octagons, PAE 255 vs cairo);
                // agg::conv_curve subdivides them into line segments first.
                agg::conv_curve<agg::path_storage> curved_path(p);
                agg::conv_transform<agg::conv_curve<agg::path_storage>> trans_path(curved_path,
                                                                                   ctm);

                // Paint: solid color or gradient span pipeline
                agg::rgba8 color(0, 0, 0, 255);
                const vgcpu::Paint* gradient_paint = nullptr;
                if (current_paint_id < scene.paints.size()) {
                    const auto& paint = scene.paints[current_paint_id];
                    if (paint.type == ir::PaintType::kSolid) {
                        uint32_t c = paint.color;
                        color = agg::rgba8(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF,
                                           (c >> 24) & 0xFF);
                    } else {
                        gradient_paint = &paint;
                    }
                }

                if (op == ir::Opcode::kFillPath) {
                    ras.add_path(trans_path);
                    if (current_fill_rule == ir::FillRule::kEvenOdd)
                        ras.filling_rule(agg::fill_even_odd);
                    else
                        ras.filling_rule(agg::fill_non_zero);

                    if (gradient_paint != nullptr) {
                        RenderGradientFill(ras, sl, ren_base, *gradient_paint);
                    } else {
                        agg::render_scanlines_aa_solid(ras, sl, ren_base, color);
                    }
                } else {
                    // Stroke
                    agg::conv_stroke<agg::conv_transform<agg::conv_curve<agg::path_storage>>>
                        stroke(trans_path);
                    stroke.width(current_stroke_width);
                    // TODO: Caps/Joins from current_stroke_opts

                    ras.add_path(stroke);
                    agg::render_scanlines_aa_solid(ras, sl, ren_base, color);
                }
                ras.reset();
                break;
            }
        }
    }

    return Status::Ok();
}

void RegisterAggAdapter() {
    AdapterRegistry::Instance().Register("agg", "Anti-Grain Geometry 2.6",
                                         []() { return std::make_unique<AggAdapter>(); });
}

}  // namespace vgcpu::adapters::agg_backend
