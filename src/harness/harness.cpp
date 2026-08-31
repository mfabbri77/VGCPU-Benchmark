// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-08] Benchmark Harness (Chapter 3) / [API-06-06] Harness: run
// orchestration (Chapter 4)

#include "harness/harness.h"

#include "harness/statistics.h"
#include "pal/timer.h"
#include "vgcpu/artifacts/naming.hpp"
#include "vgcpu/artifacts/png_reader.hpp"
#include "vgcpu/artifacts/png_writer.hpp"
#include "vgcpu/artifacts/ssim_compare.hpp"
#include "vgcpu/internal/log.h"

#include <algorithm>
#include <filesystem>
#include <mutex>

namespace vgcpu {

CaseResult Harness::RunCase(IBackendAdapter& adapter, const PreparedScene& scene,
                            const BenchmarkPolicy& policy) {
    CaseRun run = BeginCase(adapter, scene, policy);
    int repetitions = policy.repetitions > 0 ? policy.repetitions : 1;
    for (int rep = 0; rep < repetitions; ++rep) {
        MeasureRepetition(run, policy);
    }
    return FinishCase(run, policy);
}

CaseRun Harness::BeginCase(IBackendAdapter& adapter, const PreparedScene& scene,
                           const BenchmarkPolicy& policy) {
    CaseRun run;
    run.adapter = &adapter;
    run.scene = &scene;
    run.result.backend_id = adapter.GetInfo().id;
    run.result.scene_id = scene.scene_id;
    run.result.scene_hash = scene.scene_hash;
    run.result.width = static_cast<int>(scene.width);
    run.result.height = static_cast<int>(scene.height);

    // Check compatibility
    auto caps = adapter.GetCapabilities();

    // [REQ-35] Concurrency enforcement
    if (policy.thread_count > 1 && !caps.supports_parallel_render) {
        run.result.decision = CaseDecision::kSkip;
        run.result.reasons.push_back("UNSUPPORTED_FEATURE:parallel_render");
        return run;
    }

    // Feature gating: requirements extracted from the command stream at
    // Prepare time (IR v1.1); replaces the former dead TODO.
    const RequiredFeatures& required = scene.required;
    std::string compat_reason = CheckCompatibility(caps, required);
    if (!compat_reason.empty()) {
        if (compat_reason.rfind("FALLBACK:", 0) == 0) {
            run.result.decision = CaseDecision::kFallback;
            run.result.reasons.push_back(compat_reason);
        } else {
            run.result.decision = CaseDecision::kSkip;
            run.result.reasons.push_back(compat_reason);
            return run;
        }
    } else {
        run.result.decision = CaseDecision::kExecute;
    }
    // [ARCH-14-F] Preparation phase
    auto prepare_status = adapter.Prepare(scene);
    if (prepare_status.failed()) {
        run.result.decision = CaseDecision::kFail;
        run.result.reasons.push_back("PREPARE_FAILED:" + prepare_status.message);
        return run;
    }

    // Setup surface config
    run.config.width = static_cast<int>(scene.width);
    run.config.height = static_cast<int>(scene.height);

    // Preallocate output buffer (outside timed section).
    // Blueprint Reference: [REQ-21] (Ch3), [REQ-71-01] (Ch5): the measured
    // loop MUST NOT perform filesystem I/O or VGCPU-side allocation.
    // NOTE: We use resize() not reserve() to ensure adapters receive a
    // correctly sized buffer. Adapters MUST NOT call resize/fill
    // themselves; the IR kClear command handles clearing.
    run.output_buffer.resize(static_cast<size_t>(run.config.width) * run.config.height * 4);

    // Warm-up phase for both Pre-baked and Full-lifecycle modes
    // Blueprint Reference: [ARCH-13-02a] Warmup loop (Chapter 3)
    for (int i = 0; i < policy.warmup_iterations; ++i) {
        auto status = adapter.Render(scene, run.config, run.output_buffer);
        if (status.failed()) {
            run.result.decision = CaseDecision::kFail;
            run.result.reasons.push_back("WARMUP_FAILED:" + status.message);
            return run;
        }
        auto lc_status = adapter.RenderLifecycle(scene, run.config, run.output_buffer);
        if (lc_status.failed()) {
            run.result.decision = CaseDecision::kFail;
            run.result.reasons.push_back("LIFECYCLE_WARMUP_FAILED:" + lc_status.message);
            return run;
        }
    }

    int repetitions = policy.repetitions > 0 ? policy.repetitions : 1;
    size_t total_samples =
        static_cast<size_t>(policy.measurement_iterations) * static_cast<size_t>(repetitions);
    run.wall_samples.reserve(total_samples);
    run.cpu_samples.reserve(total_samples);
    run.lifecycle_wall_samples.reserve(total_samples);
    run.lifecycle_cpu_samples.reserve(total_samples);
    run.active = true;
    return run;
}

// Measurement phase: `repetitions` measured blocks of
// `measurement_iterations` samples each, aggregated into one pool
// (sample_count = iterations * repetitions). Warmup runs once, in
// BeginCase. Scheduling is repetition-major across backends (owner
// policy, 2026-08-30; see CaseRun) -- the caller interleaves.
// Blueprint Reference: [ARCH-13-02b] Measured loop (Chapter 3)
void Harness::MeasureRepetition(CaseRun& run, const BenchmarkPolicy& policy) {
    if (!run.active) {
        return;
    }
    // Mode A: Pre-baked (draw time only)
    for (int i = 0; i < policy.measurement_iterations; ++i) {
        auto cpu_start = pal::GetCpuTime();
        auto wall_start = pal::NowMonotonic();

        auto status = run.adapter->Render(*run.scene, run.config, run.output_buffer);

        auto wall_end = pal::NowMonotonic();
        auto cpu_end = pal::GetCpuTime();

        if (status.failed()) {
            run.result.decision = CaseDecision::kFail;
            run.result.reasons.push_back("RENDER_FAILED:" + status.message);
            run.active = false;
            return;
        }

        run.wall_samples.push_back(pal::ToNanoseconds(pal::Elapsed(wall_start, wall_end)));
        run.cpu_samples.push_back(pal::ToNanoseconds(cpu_end - cpu_start));
    }

    // Mode B: Full-lifecycle (Loop 1: Create -> Loop 2: Draw -> Loop 3: Destroy)
    for (int i = 0; i < policy.measurement_iterations; ++i) {
        auto cpu_start = pal::GetCpuTime();
        auto wall_start = pal::NowMonotonic();

        auto status = run.adapter->RenderLifecycle(*run.scene, run.config, run.output_buffer);

        auto wall_end = pal::NowMonotonic();
        auto cpu_end = pal::GetCpuTime();

        if (status.failed()) {
            run.result.decision = CaseDecision::kFail;
            run.result.reasons.push_back("LIFECYCLE_RENDER_FAILED:" + status.message);
            run.active = false;
            return;
        }

        run.lifecycle_wall_samples.push_back(
            pal::ToNanoseconds(pal::Elapsed(wall_start, wall_end)));
        run.lifecycle_cpu_samples.push_back(pal::ToNanoseconds(cpu_end - cpu_start));
    }
}

CaseResult Harness::FinishCase(CaseRun& run, const BenchmarkPolicy& policy) {
    if (!run.active) {
        return run.result;
    }
    CaseResult& result = run.result;

    // Compute statistics for both modes
    result.stats = ComputeStats(run.wall_samples, run.cpu_samples);
    result.lifecycle_stats = ComputeStats(run.lifecycle_wall_samples, run.lifecycle_cpu_samples);
    if (result.decision != CaseDecision::kFallback) {
        result.decision = CaseDecision::kExecute;
    }

    // Artifact Generation
    if (policy.generate_png) {
        // [CONC-08-01] Serialize artifact I/O
        static std::mutex artifact_mutex;
        std::lock_guard<std::mutex> lock(artifact_mutex);

        std::string filename =
            artifacts::generate_artifact_path(result.backend_id, result.scene_id, ".png");
        std::filesystem::path out_path = std::filesystem::path(policy.output_dir) / filename;

        // Ensure output dir exists
        std::error_code ec;
        std::filesystem::create_directories(out_path.parent_path(), ec);

        if (artifacts::write_png(out_path.string(), result.width, result.height,
                                 run.output_buffer)) {
            result.artifact_path = out_path.string();
        } else {
            VGCPU_LOG_ERROR("Failed to write artifact: " + out_path.string());
        }
    }

    // SSIM Comparison
    if (policy.compare_ssim) {
        std::string filename =
            artifacts::generate_artifact_path(result.backend_id, result.scene_id, ".png");

        std::filesystem::path golden_path = std::filesystem::path(policy.golden_dir) / filename;
        result.golden_path = golden_path.string();

        if (std::filesystem::exists(golden_path)) {
            int gw = 0, gh = 0;
            auto golden_pixels = artifacts::read_image(golden_path.string(), gw, gh);
            if (!golden_pixels.empty()) {
                if (gw == result.width && gh == result.height) {
                    auto ssim_res = artifacts::compute_ssim(gw, gh, run.output_buffer, gw * 4,
                                                            golden_pixels, gw * 4);
                    result.ssim_score = ssim_res.score;
                    result.ssim_passed = ssim_res.passed;
                    result.ssim_message = ssim_res.message;
                    result.ssim_pae = ssim_res.pae;
                    result.ssim_ae_ratio = ssim_res.ae_ratio;
                } else {
                    result.ssim_passed = false;
                    result.ssim_message = "Dimension mismatch";
                }
            } else {
                result.ssim_passed = false;
                result.ssim_message = "Failed to load golden image";
            }
        } else {
            result.ssim_message = "Golden image not found";
        }
    }

    run.active = false;
    return result;
}

std::string Harness::CheckCompatibility(const CapabilitySet& caps,
                                        const RequiredFeatures& required) {
    return vgcpu::CheckCompatibility(caps, required);
}

}  // namespace vgcpu
