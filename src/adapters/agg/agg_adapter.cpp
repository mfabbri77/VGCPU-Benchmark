/*
 * Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
 * SPDX-License-Identifier: MIT
 */
#include "adapters/agg/agg_adapter.h"

#include "adapters/adapter_registry.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 5054 5055)
#endif

#include "agg_conv_curve.h"
#include "agg_conv_dash.h"
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

using AggLut = agg::gradient_lut<agg::color_interpolator<agg::rgba8>, 256>;

struct AggPaint {
    ir::PaintType type = ir::PaintType::kSolid;
    agg::rgba8 color;
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f, r = 0.0f;
    std::unique_ptr<AggLut> lut;
};

namespace {
template <typename T>
T ReadLE(const uint8_t*& ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    ptr += sizeof(T);
    return val;
}
std::shared_ptr<AggPaint> CreateAggPaint(const vgcpu::Paint& p) {
    auto ap = std::make_shared<AggPaint>();
    ap->type = p.type;
    if (p.type == ir::PaintType::kSolid) {
        uint32_t c = p.color;
        ap->color = agg::rgba8(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF);
    } else if (p.type == ir::PaintType::kLinear) {
        ap->x0 = p.linear_start_x;
        ap->y0 = p.linear_start_y;
        ap->x1 = p.linear_end_x;
        ap->y1 = p.linear_end_y;
        ap->lut = std::make_unique<AggLut>();
        for (const auto& s : p.stops) {
            ap->lut->add_color(
                s.offset, agg::rgba8(s.color & 0xFF, (s.color >> 8) & 0xFF, (s.color >> 16) & 0xFF,
                                     (s.color >> 24) & 0xFF));
        }
        ap->lut->build_lut();
    } else if (p.type == ir::PaintType::kRadial) {
        ap->x0 = p.radial_center_x;
        ap->y0 = p.radial_center_y;
        ap->r = p.radial_radius;
        ap->lut = std::make_unique<AggLut>();
        for (const auto& s : p.stops) {
            ap->lut->add_color(
                s.offset, agg::rgba8(s.color & 0xFF, (s.color >> 8) & 0xFF, (s.color >> 16) & 0xFF,
                                     (s.color >> 24) & 0xFF));
        }
        ap->lut->build_lut();
    }
    return ap;
}

template <typename Rasterizer, typename Scanline, typename RenBase>
void RenderGradientFill(Rasterizer& ras, Scanline& sl, RenBase& ren_base, const AggPaint& paint,
                        const agg::trans_affine& ctm) {
    agg::span_allocator<agg::rgba8> alloc;
    using LutType = AggLut;
    const LutType& lut = *paint.lut;

    if (paint.type == ir::PaintType::kLinear) {
        double dx = static_cast<double>(paint.x1) - paint.x0;
        double dy = static_cast<double>(paint.y1) - paint.y0;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) {
            len = 1e-6;
        }
        agg::trans_affine mtx;
        mtx *= agg::trans_affine_rotation(std::atan2(dy, dx));
        mtx *= agg::trans_affine_translation(paint.x0, paint.y0);
        mtx *= ctm;
        mtx.invert();
        agg::span_interpolator_linear<> inter(mtx);
        agg::gradient_x gfunc;
        agg::span_gradient<agg::rgba8, agg::span_interpolator_linear<>, agg::gradient_x, LutType>
            sg(inter, gfunc, const_cast<LutType&>(lut), 0.0, len);
        agg::render_scanlines_aa(ras, sl, ren_base, alloc, sg);
    } else {
        double radius = paint.r > 1e-6f ? paint.r : 1e-6;
        agg::trans_affine mtx;
        mtx *= agg::trans_affine_translation(paint.x0, paint.y0);
        mtx *= ctm;
        mtx.invert();
        agg::span_interpolator_linear<> inter(mtx);
        agg::gradient_radial_d gfunc;
        agg::span_gradient<agg::rgba8, agg::span_interpolator_linear<>, agg::gradient_radial_d,
                           LutType>
            sg(inter, gfunc, const_cast<LutType&>(lut), 0.0, radius);
        agg::render_scanlines_aa(ras, sl, ren_base, alloc, sg);
    }
}

void PopulateAggPath(const Path& ir_path, agg::path_storage& p) {
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
}

agg::path_storage CreateAggPath(const Path& ir_path) {
    agg::path_storage p;
    PopulateAggPath(ir_path, p);
    return p;
}

template <typename PathGetter, typename RenBase>
Status ExecuteAggCommands(const PreparedScene& scene, const SurfaceConfig& config,
                          PathGetter&& get_path,
                          const std::vector<std::shared_ptr<AggPaint>>& paints, RenBase& ren_base) {
    uint32_t width = config.width;
    uint32_t height = config.height;

    agg::rasterizer_scanline_aa<> ras;
    agg::scanline_p8 sl;
    agg::trans_affine ctm;

    const uint8_t* ptr = scene.command_stream.data();
    const uint8_t* end = ptr + scene.command_stream.size();

    uint16_t current_paint_id = 0xFFFF;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    float current_stroke_width = 1.0f;
    uint8_t current_stroke_opts = 0;
    uint16_t current_stroke_paint_id = 0;
    std::vector<float> dash_lengths;
    float dash_phase = 0.0f;

    struct RectClip {
        float x1, y1, x2, y2;
    };
    std::vector<RectClip> clip_stack;
    ras.clip_box(0, 0, width, height);

    while (ptr < end) {
        ir::Opcode op = static_cast<ir::Opcode>(*ptr++);

        switch (op) {
            case ir::Opcode::kEnd:
                return Status::Ok();

            case ir::Opcode::kClear: {
                uint32_t c = ReadLE<uint32_t>(ptr);
                uint8_t r = c & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = (c >> 16) & 0xFF;
                uint8_t a = (c >> 24) & 0xFF;
                ren_base.clear(agg::rgba8(r, g, b, a));
                break;
            }

            case ir::Opcode::kSetMatrix: {
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
                current_stroke_paint_id = ReadLE<uint16_t>(ptr);
                current_stroke_width = ReadLE<float>(ptr);
                current_stroke_opts = *ptr++;
                break;
            }

            case ir::Opcode::kSave:
            case ir::Opcode::kRestore:
                break;

            case ir::Opcode::kFillPath:
            case ir::Opcode::kStrokePath: {
                uint16_t path_id = ReadLE<uint16_t>(ptr);
                const agg::path_storage* path_ptr = get_path(path_id);
                if (!path_ptr)
                    break;

                agg::conv_curve<agg::path_storage> curved_path(
                    const_cast<agg::path_storage&>(*path_ptr));
                agg::conv_transform<agg::conv_curve<agg::path_storage>> trans_path(curved_path,
                                                                                   ctm);
                uint16_t paint_id =
                    (op == ir::Opcode::kFillPath) ? current_paint_id : current_stroke_paint_id;
                if (paint_id >= paints.size() || !paints[paint_id])
                    break;
                const auto& cur_paint = *paints[paint_id];

                if (op == ir::Opcode::kFillPath) {
                    ras.add_path(trans_path);
                    if (current_fill_rule == ir::FillRule::kEvenOdd)
                        ras.filling_rule(agg::fill_even_odd);
                    else
                        ras.filling_rule(agg::fill_non_zero);

                    if (cur_paint.type != ir::PaintType::kSolid && cur_paint.lut) {
                        RenderGradientFill(ras, sl, ren_base, cur_paint, ctm);
                    } else {
                        agg::render_scanlines_aa_solid(ras, sl, ren_base, cur_paint.color);
                    }
                } else {
                    if (!dash_lengths.empty()) {
                        agg::conv_dash<agg::conv_transform<agg::conv_curve<agg::path_storage>>>
                            dashed(trans_path);
                        for (size_t di = 0; di + 1 < dash_lengths.size(); di += 2) {
                            dashed.add_dash(dash_lengths[di], dash_lengths[di + 1]);
                        }
                        dashed.dash_start(dash_phase);
                        agg::conv_stroke<
                            agg::conv_dash<agg::conv_transform<agg::conv_curve<agg::path_storage>>>>
                            dash_stroke(dashed);
                        dash_stroke.width(current_stroke_width);
                        switch (ir::UnpackStrokeCap(current_stroke_opts)) {
                            case ir::StrokeCap::kButt:
                                dash_stroke.line_cap(agg::butt_cap);
                                break;
                            case ir::StrokeCap::kRound:
                                dash_stroke.line_cap(agg::round_cap);
                                break;
                            case ir::StrokeCap::kSquare:
                                dash_stroke.line_cap(agg::square_cap);
                                break;
                        }
                        switch (ir::UnpackStrokeJoin(current_stroke_opts)) {
                            case ir::StrokeJoin::kMiter:
                                dash_stroke.line_join(agg::miter_join);
                                break;
                            case ir::StrokeJoin::kRound:
                                dash_stroke.line_join(agg::round_join);
                                break;
                            case ir::StrokeJoin::kBevel:
                                dash_stroke.line_join(agg::bevel_join);
                                break;
                        }
                        ras.add_path(dash_stroke);
                    } else {
                        agg::conv_stroke<agg::conv_transform<agg::conv_curve<agg::path_storage>>>
                            stroke(trans_path);
                        stroke.width(current_stroke_width);
                        switch (ir::UnpackStrokeCap(current_stroke_opts)) {
                            case ir::StrokeCap::kButt:
                                stroke.line_cap(agg::butt_cap);
                                break;
                            case ir::StrokeCap::kRound:
                                stroke.line_cap(agg::round_cap);
                                break;
                            case ir::StrokeCap::kSquare:
                                stroke.line_cap(agg::square_cap);
                                break;
                        }
                        switch (ir::UnpackStrokeJoin(current_stroke_opts)) {
                            case ir::StrokeJoin::kMiter:
                                stroke.line_join(agg::miter_join);
                                break;
                            case ir::StrokeJoin::kRound:
                                stroke.line_join(agg::round_join);
                                break;
                            case ir::StrokeJoin::kBevel:
                                stroke.line_join(agg::bevel_join);
                                break;
                        }
                        ras.add_path(stroke);
                    }
                    if (cur_paint.type != ir::PaintType::kSolid && cur_paint.lut) {
                        RenderGradientFill(ras, sl, ren_base, cur_paint, ctm);
                    } else {
                        agg::render_scanlines_aa_solid(ras, sl, ren_base, cur_paint.color);
                    }
                }
                ras.reset();
                break;
            }

            case ir::Opcode::kSetDash: {
                if (ptr + 5 > end)
                    return Status::Ok();
                uint8_t count = *ptr++;
                dash_phase = ReadLE<float>(ptr);
                if (ptr + 4 * count > end)
                    return Status::Ok();
                dash_lengths.clear();
                for (uint8_t i = 0; i < count; ++i) {
                    dash_lengths.push_back(ReadLE<float>(ptr));
                }
                break;
            }

            case ir::Opcode::kClipPush: {
                if (ptr + 3 > end)
                    return Status::Ok();
                uint16_t path_id = ReadLE<uint16_t>(ptr);
                ptr += 1;
                const agg::path_storage* cp = get_path(path_id);
                if (cp) {
                    double x1 = 1e9, y1 = 1e9, x2 = -1e9, y2 = -1e9;
                    for (unsigned vi = 0; vi < cp->total_vertices(); ++vi) {
                        double px = 0, py = 0;
                        cp->vertex(vi, &px, &py);
                        if (px < x1)
                            x1 = px;
                        if (px > x2)
                            x2 = px;
                        if (py < y1)
                            y1 = py;
                        if (py > y2)
                            y2 = py;
                    }
                    float fx1 = (float)x1, fy1 = (float)y1, fx2 = (float)x2, fy2 = (float)y2;
                    if (!clip_stack.empty()) {
                        const auto& top = clip_stack.back();
                        if (top.x1 > fx1)
                            fx1 = top.x1;
                        if (top.y1 > fy1)
                            fy1 = top.y1;
                        if (top.x2 < fx2)
                            fx2 = top.x2;
                        if (top.y2 < fy2)
                            fy2 = top.y2;
                    }
                    clip_stack.push_back({fx1, fy1, fx2, fy2});
                    ras.clip_box(fx1, fy1, fx2, fy2);
                }
                break;
            }

            case ir::Opcode::kClipPop: {
                if (!clip_stack.empty())
                    clip_stack.pop_back();
                if (!clip_stack.empty()) {
                    const auto& top = clip_stack.back();
                    ras.clip_box(top.x1, top.y1, top.x2, top.y2);
                } else {
                    ras.clip_box(0, 0, width, height);
                }
                break;
            }

            default:
                break;
        }
    }
    return Status::Ok();
}

}  // namespace

Status AggAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                          std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("AggAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    using PixFmt = agg::pixfmt_rgba32_plain;
    using RenBase = agg::renderer_base<PixFmt>;

    agg::rendering_buffer rbuf(output_buffer.data(), config.width, config.height, config.width * 4);
    PixFmt pixf(rbuf);
    RenBase ren_base(pixf);

    auto get_path = [&](uint16_t id) -> const agg::path_storage* {
        return (id < prepared_paths_.size()) ? &prepared_paths_[id] : nullptr;
    };
    return ExecuteAggCommands(scene, config, get_path, prepared_paints_, ren_base);
}
Status AggAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                   std::vector<uint8_t>& output_buffer) {
    if (!initialized_) {
        return Status::Fail("AggAdapter not initialized");
    }
    if (!scene.IsValid()) {
        return Status::InvalidArg("Invalid scene");
    }
    if (config.width <= 0 || config.height <= 0) {
        return Status::InvalidArg("Invalid surface configuration");
    }

    using PixFmt = agg::pixfmt_rgba32_plain;
    using RenBase = agg::renderer_base<PixFmt>;

    agg::rendering_buffer rbuf(output_buffer.data(), config.width, config.height, config.width * 4);
    PixFmt pixf(rbuf);
    RenBase ren_base(pixf);

    std::vector<std::shared_ptr<AggPaint>> paints;
    paints.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        paints.push_back(CreateAggPaint(p));
    }

    thread_local agg::path_storage scratch_path;
    auto get_path = [&](uint16_t id) -> const agg::path_storage* {
        if (id >= scene.paths.size())
            return nullptr;
        scratch_path.remove_all();
        PopulateAggPath(scene.paths[id], scratch_path);
        return &scratch_path;
    };

    Status s = ExecuteAggCommands(scene, config, get_path, paints, ren_base);
    paints.clear();
    return s;
}

AggAdapter::AggAdapter() = default;
AggAdapter::~AggAdapter() = default;

Status AggAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    initialized_ = true;
    return Status::Ok();
}

Status AggAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_) {
        return Status::Fail("AggAdapter not initialized");
    }
    prepared_paths_.clear();
    prepared_paths_.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        prepared_paths_.push_back(CreateAggPath(p));
    }
    prepared_paints_.clear();
    prepared_paints_.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        prepared_paints_.push_back(CreateAggPaint(p));
    }
    return Status::Ok();
}

void AggAdapter::Shutdown() {
    prepared_paths_.clear();
    prepared_paints_.clear();
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

void RegisterAggAdapter() {
    AdapterRegistry::Instance().Register("agg", "Anti-Grain Geometry 2.6",
                                         []() { return std::make_unique<AggAdapter>(); });
}

}  // namespace vgcpu::adapters::agg_backend
