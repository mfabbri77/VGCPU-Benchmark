// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-08] Benchmark Harness (Chapter 3) / [API-06-06] Harness: run
// orchestration (Chapter 4)

#include "harness/harness.h"

#include "harness/statistics.h"
#include "pal/timer.h"

#include <algorithm>

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
    // Blueprint Reference: [REQ-89] The measured loop MUST NOT perform filesystem I/O (Chapter 4?
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

    return result;
}

std::string Harness::CheckCompatibility(const CapabilitySet& caps,
                                        const RequiredFeatures& required) {
    return vgcpu::CheckCompatibility(caps, required);
}

}  // namespace vgcpu
