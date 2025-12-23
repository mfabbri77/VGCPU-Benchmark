// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-10] Reporting (Chapter 3) / [API-06-07] WriteSummaryToStdout
// (Chapter 4)

#include "reporting/reporter.h"

#include <iomanip>
#include <iostream>

namespace vgcpu {

namespace {

std::string DecisionToString(CaseDecision decision) {
    switch (decision) {
        case CaseDecision::kExecute:
            return "OK";
        case CaseDecision::kSkip:
            return "SKIP";
        case CaseDecision::kFail:
            return "FAIL";
        case CaseDecision::kFallback:
            return "FALLBACK";
    }
    return "?";
}

double NsToMs(int64_t ns) {
    return static_cast<double>(ns) / 1'000'000.0;
}

}  // namespace

void SummaryWriter::PrintSummary(const RunMetadata& metadata,
                                 const std::vector<CaseResult>& results) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║            VGCPU-Benchmark Results Summary                       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    // Environment info
    std::cout << "Environment:\n";
    std::cout << "  OS:        " << metadata.environment.os_name << " "
              << metadata.environment.os_version << "\n";
    std::cout << "  Arch:      " << metadata.environment.arch << "\n";
    std::cout << "  CPU:       " << metadata.environment.cpu_model << "\n";
    std::cout << "  Cores:     " << metadata.environment.cpu_cores << "\n";
    std::cout << "  Compiler:  " << metadata.environment.compiler_name << " "
              << metadata.environment.compiler_version << "\n";
    std::cout << "  Timestamp: " << metadata.run_timestamp << "\n\n";

    // Policy info
    std::cout << "Benchmark Policy:\n";
    std::cout << "  Warmup:      " << metadata.policy.warmup_iterations << " iterations\n";
    std::cout << "  Measurement: " << metadata.policy.measurement_iterations << " iterations\n";
    std::cout << "  Repetitions: " << metadata.policy.repetitions << "\n\n";

    // Count outcomes
    int executed = 0, skipped = 0, failed = 0;
    for (const auto& r : results) {
        switch (r.decision) {
            case CaseDecision::kExecute:
                ++executed;
                break;
            case CaseDecision::kSkip:
                ++skipped;
                break;
            case CaseDecision::kFail:
                ++failed;
                break;
            default:
                break;
        }
    }

    std::cout << "Results: " << executed << " executed, " << skipped << " skipped, " << failed
              << " failed / " << results.size() << " total\n\n";

    // Results table
    if (!results.empty()) {
        std::cout << std::left << std::setw(12) << "Backend" << std::setw(24) << "Scene"
                  << std::setw(8) << "Status" << std::right << std::setw(12) << "Wall p50"
                  << std::setw(12) << "CPU p50"
                  << "\n";
        std::cout << std::string(68, '-') << "\n";

        for (const auto& r : results) {
            std::cout << std::left << std::setw(12) << r.backend_id << std::setw(24) << r.scene_id
                      << std::setw(8) << DecisionToString(r.decision);

            if (r.decision == CaseDecision::kExecute) {
                std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(10)
                          << NsToMs(r.stats.wall_p50_ns) << "ms" << std::setw(10)
                          << NsToMs(r.stats.cpu_p50_ns) << "ms";
            } else if (!r.reasons.empty()) {
                std::cout << "  (" << r.reasons[0] << ")";
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n";
}

}  // namespace vgcpu
