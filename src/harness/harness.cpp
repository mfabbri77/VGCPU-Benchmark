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
    CaseResult result;
    result.backend_id = adapter.GetInfo().id;
    result.scene_id = scene.scene_id;
    result.scene_hash = scene.scene_hash;
    result.width = static_cast<int>(scene.width);
    result.height = static_cast<int>(scene.height);

    // Check compatibility
    auto caps = adapter.GetCapabilities();

    // [REQ-35] Concurrency enforcement
    if (policy.thread_count > 1 && !caps.supports_parallel_render) {
        result.decision = CaseDecision::kSkip;
        result.reasons.push_back("UNSUPPORTED_FEATURE:parallel_render");
        return result;
    }

    RequiredFeatures required;  // TODO: Extract from scene
    std::string compat_reason = CheckCompatibility(caps, required);
    if (!compat_reason.empty()) {
        result.decision = CaseDecision::kSkip;
        result.reasons.push_back(compat_reason);
        return result;
    }

    // [ARCH-14-F] Preparation phase
    auto prepare_status = adapter.Prepare(scene);
    if (prepare_status.failed()) {
        result.decision = CaseDecision::kFail;
        result.reasons.push_back("PREPARE_FAILED:" + prepare_status.message);
        return result;
    }

    // Setup surface config
    SurfaceConfig config;
    config.width = static_cast<int>(scene.width);
    config.height = static_cast<int>(scene.height);

    // Preallocate output buffer (outside timed section)
    // Blueprint Reference: [REQ-21] Measured loop MUST NOT perform filesystem I/O (Chapter 3)

    // Wait, it was REQ-21 in Ch3) Actually [REQ-21] in Ch3 is the correct one. [REQ-89] was from an
    // older version or I misread. Let's use [REQ-21] (Ch3) and [REQ-71-01] (Ch5). NOTE: We use
    // resize() not reserve() to ensure adapters receive a correctly sized buffer. Adapters MUST NOT
    // call resize/fill themselves; the IR kClear command handles clearing.
    std::vector<uint8_t> output_buffer;
    output_buffer.resize(static_cast<size_t>(config.width) * config.height * 4);

    // Warm-up phase (untimed for primary stats)
    // Blueprint Reference: [ARCH-13-02a] Warmup loop (Chapter 3)
    for (int i = 0; i < policy.warmup_iterations; ++i) {
        auto status = adapter.Render(scene, config, output_buffer);
        if (status.failed()) {
            result.decision = CaseDecision::kFail;
            result.reasons.push_back("WARMUP_FAILED:" + status.message);
            return result;
        }
    }

    // Measurement phase
    // Blueprint Reference: [ARCH-13-02b] Measured loop (Chapter 3) / [REQ-21,22,23] (Chapter 3)
    std::vector<int64_t> wall_samples;
    std::vector<int64_t> cpu_samples;
    wall_samples.reserve(static_cast<size_t>(policy.measurement_iterations));
    cpu_samples.reserve(static_cast<size_t>(policy.measurement_iterations));

    // Note: Overhead measurement removed for now - not actively used

    for (int i = 0; i < policy.measurement_iterations; ++i) {
        // Measure inter-call overhead (from end of last iteration to start of this one)
        // Note: This is an approximation.

        // Start timing
        auto cpu_start = pal::GetCpuTime();
        auto wall_start = pal::NowMonotonic();

        // Timed section: ONLY rendering
        auto status = adapter.Render(scene, config, output_buffer);

        // End timing
        auto wall_end = pal::NowMonotonic();
        auto cpu_end = pal::GetCpuTime();

        if (status.failed()) {
            result.decision = CaseDecision::kFail;
            result.reasons.push_back("RENDER_FAILED:" + status.message);
            return result;
        }

        wall_samples.push_back(pal::ToNanoseconds(pal::Elapsed(wall_start, wall_end)));
        cpu_samples.push_back(pal::ToNanoseconds(cpu_end - cpu_start));
    }

    // Compute statistics
    result.stats = ComputeStats(wall_samples, cpu_samples);
    result.decision = CaseDecision::kExecute;

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

        if (artifacts::write_png(out_path.string(), result.width, result.height, output_buffer)) {
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
                    auto ssim_res = artifacts::compute_ssim(gw, gh, output_buffer, gw * 4,
                                                            golden_pixels, gw * 4);
                    result.ssim_score = ssim_res.score;
                    result.ssim_passed = ssim_res.passed;
                    result.ssim_message = ssim_res.message;
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

    return result;
}

std::string Harness::CheckCompatibility(const CapabilitySet& caps,
                                        const RequiredFeatures& required) {
    return vgcpu::CheckCompatibility(caps, required);
}

}  // namespace vgcpu
