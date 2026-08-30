// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-03] PAL (Chapter 3) / [API-06-02] PAL (Chapter 4)
// Blueprint Reference: [ARCH-12-02d] RunReport environment metadata (Chapter 3)

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vgcpu {
namespace pal {

/// Environment information for run metadata.
/// Blueprint Reference: [ARCH-12-02d] RunReport environment metadata (Chapter 3) / [API-06-02]
/// EnvInfo (Chapter 4)
struct EnvironmentInfo {
    std::string os_name;
    std::string os_version;
    std::string arch;
    std::string cpu_model;
    int cpu_cores = 0;
    int64_t memory_bytes = 0;
    std::string compiler_name;
    std::string compiler_version;
    // Measurement discipline (REQ-13-03): filled by the CLI when --pin is
    // used. Empty = not pinned; otherwise the cpuset string as given
    // (e.g. "2" or "0-11"). cpu_governor is empty where the platform does
    // not expose one (non-Linux).
    std::string pinned_cpus;
    std::string cpu_governor;
};

/// Collect environment information for the current system.
[[nodiscard]] EnvironmentInfo CollectEnvironment();

/// Pin the current process (calling thread + threads created afterwards)
/// to a set of logical CPUs. Best-effort per platform (REQ-13-03): Linux
/// and Windows implement it; elsewhere returns false. Empty set fails.
[[nodiscard]] bool PinToCpus(const std::vector<int>& cpus);

/// Read the cpufreq scaling governor of a logical CPU. Linux only; returns
/// an empty string where unavailable.
[[nodiscard]] std::string GetCpuGovernor(int cpu);

/// Get the current timestamp in ISO 8601 format.
[[nodiscard]] std::string GetTimestamp();

}  // namespace pal
}  // namespace vgcpu
