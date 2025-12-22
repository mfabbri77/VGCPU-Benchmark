// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.2 — Module harness

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
/// Blueprint Reference: Chapter 6, §6.1.2 — Benchmark Policy options
struct BenchmarkPolicy {
    int warmup_iterations = 3;
    int measurement_iterations = 10;
    int repetitions = 1;
    int thread_count = 1;  // 0 = backend default
};

/// Timing statistics for a single benchmark case.
/// Blueprint Reference: Chapter 6, §6.5.2 — Measurement Record Schema
struct TimingStats {
    int64_t wall_p50_ns = 0;  ///< Median wall time in nanoseconds
    int64_t wall_p90_ns = 0;  ///< 90th percentile wall time
    int64_t cpu_p50_ns = 0;   ///< Median CPU time in nanoseconds
    int64_t cpu_p90_ns = 0;   ///< 90th percentile CPU time
    int sample_count = 0;     ///< Number of samples
};

/// Execution outcome for a benchmark case.
/// Blueprint Reference: Chapter 6, §6.4.2 — Compatibility Decision Output
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

    TimingStats stats;
};

/// Full benchmark run result.
/// Blueprint Reference: Chapter 6, §6.6.1 — Machine-Readable Output
struct RunResult {
    std::string run_timestamp;
    BenchmarkPolicy policy;
    std::vector<CaseResult> cases;
};

/// Harness for executing benchmarks.
/// Blueprint Reference: Chapter 7, §7.2.2 — Harness Core
class Harness {
   public:
    /// Run a benchmark for a single scene on a single backend.
    /// @param adapter The backend adapter to use.
    /// @param scene The prepared scene to benchmark.
    /// @param policy Benchmark configuration.
    /// @return Case result with timing statistics.
    static CaseResult RunCase(IBackendAdapter& adapter, const PreparedScene& scene,
                              const BenchmarkPolicy& policy);

    /// Check if a scene is compatible with a backend.
    /// @param caps Backend capabilities.
    /// @param required Scene feature requirements.
    /// @return Empty string if compatible, or reason code if not.
    static std::string CheckCompatibility(const CapabilitySet& caps,
                                          const RequiredFeatures& required);
};

}  // namespace vgcpu
