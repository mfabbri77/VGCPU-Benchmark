// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-08] Benchmark Harness (Chapter 3) / [API-06-06] Harness: run
// orchestration (Chapter 4)

#pragma once

#include "adapters/adapter_interface.h"
#include "common/capability_set.h"
#include "common/status.h"
#include "ir/prepared_scene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vgcpu {

/// Benchmark policy configuration.
/// Blueprint Reference: [ARCH-12-02a] RunConfig (Chapter 3) / [ARCH-14-A] CLI Frontend (Chapter 3)
struct BenchmarkPolicy {
    int warmup_iterations = 3;
    int measurement_iterations = 10;
    int repetitions = 1;
    int thread_count = 1;  // 0 = backend default
    bool generate_png = false;
    bool compare_ssim = false;
    std::string golden_dir;
    std::string output_dir = ".";
};

/// Timing statistics for a single benchmark case.
/// Blueprint Reference: [ARCH-12-02c] SceneStats (Chapter 3) / [ARCH-12-02d] RunReport (Chapter 3)
struct TimingStats {
    int64_t wall_p50_ns = 0;  ///< Median wall time in nanoseconds
    int64_t wall_p90_ns = 0;  ///< 90th percentile wall time
    int64_t cpu_p50_ns = 0;   ///< Median CPU time in nanoseconds
    int64_t cpu_p90_ns = 0;   ///< 90th percentile CPU time
    int sample_count = 0;     ///< Number of samples
};

/// Execution outcome for a benchmark case.
/// Blueprint Reference: [ARCH-13-01] Run lifecycle state machine (Chapter 3) / [API-03] Error
/// handling (Chapter 4)
enum class CaseDecision {
    kExecute,   ///< Case was executed successfully
    kSkip,      ///< Case was skipped (unsupported features)
    kFail,      ///< Case failed during execution
    kFallback,  ///< Case used fallback mode
};

/// Result for a single benchmark case.
struct CaseResult {
    std::string backend_id;
    std::string scene_id;
    std::string scene_hash;
    int width = 0;
    int height = 0;

    CaseDecision decision = CaseDecision::kSkip;
    std::vector<std::string> reasons;

    TimingStats stats;            ///< Pre-baked mode (draw time only)
    TimingStats lifecycle_stats;  ///< Full-lifecycle mode (Create + Draw + Destroy)

    // Artifacts
    std::string artifact_path;
    std::string golden_path;
    double ssim_score = 0.0;
    bool ssim_passed = true;  // Default true if not run
    std::string ssim_message;
    int ssim_pae = 0;            ///< peak absolute error (L-infinity), 0..255
    double ssim_ae_ratio = 0.0;  ///< fraction of pixels over the AE tolerance
};

/// Full benchmark run result.
/// Blueprint Reference: [API-01-02] Report schemas (Chapter 4) / [REQ-48] CSV/JSON output (Chapter
/// 4)
struct RunResult {
    std::string run_timestamp;
    BenchmarkPolicy policy;
    std::vector<CaseResult> cases;
};

/// Per-case execution state for interleaved repetition scheduling.
/// Owner policy (2026-08-30): repetitions are scheduled repetition-major
/// across backends -- every backend runs repetition r before any backend
/// runs repetition r+1 -- so machine-state transients (thermal drift,
/// scheduler/HT-sibling interference) spread across all engines instead
/// of poisoning one backend's whole sample pool.
struct CaseRun {
    IBackendAdapter* adapter = nullptr;
    const PreparedScene* scene = nullptr;
    SurfaceConfig config;
    std::vector<uint8_t> output_buffer;
    std::vector<int64_t> wall_samples;            ///< Pre-baked wall time samples
    std::vector<int64_t> cpu_samples;             ///< Pre-baked CPU time samples
    std::vector<int64_t> lifecycle_wall_samples;  ///< Full-lifecycle wall time samples
    std::vector<int64_t> lifecycle_cpu_samples;   ///< Full-lifecycle CPU time samples
    CaseResult result;
    bool active = false;  ///< prepared + warmed and no failure so far
};

/// Harness for executing benchmarks.
/// Blueprint Reference: [ARCH-10-08] Benchmark Harness (Chapter 3) / [ARCH-13] Primary execution
/// flow (Chapter 3)
class Harness {
   public:
    /// Run a benchmark for a single scene on a single backend
    /// (BeginCase + repetitions x MeasureRepetition + FinishCase).
    static CaseResult RunCase(IBackendAdapter& adapter, const PreparedScene& scene,
                              const BenchmarkPolicy& policy);

    /// Phase 1: compatibility check, scene preparation, buffer setup and
    /// warmup. On any failure the returned state is inactive and already
    /// carries the final (skip/fail) result.
    static CaseRun BeginCase(IBackendAdapter& adapter, const PreparedScene& scene,
                             const BenchmarkPolicy& policy);

    /// Phase 2: one measured repetition (policy.measurement_iterations
    /// samples). No-op on inactive runs.
    static void MeasureRepetition(CaseRun& run, const BenchmarkPolicy& policy);

    /// Phase 3: statistics, artifact generation and SSIM comparison.
    static CaseResult FinishCase(CaseRun& run, const BenchmarkPolicy& policy);

    /// Check if a scene is compatible with a backend.
    /// @param caps Backend capabilities.
    /// @param required Scene feature requirements.
    /// @return Empty string if compatible, or reason code if not.
    static std::string CheckCompatibility(const CapabilitySet& caps,
                                          const RequiredFeatures& required);
};

}  // namespace vgcpu
