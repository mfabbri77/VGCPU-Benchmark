// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 6, §6.6.1 — run_metadata fields
// Blueprint Reference: Chapter 7, §7.2.3 — Environment metadata acquisition

#pragma once

#include <string>

namespace vgcpu {
namespace pal {

/// Environment information for run metadata.
/// Blueprint Reference: Chapter 6, §6.6.1 — run_metadata
struct EnvironmentInfo {
    std::string os_name;
    std::string os_version;
    std::string arch;
    std::string cpu_model;
    int cpu_cores = 0;
    int64_t memory_bytes = 0;
    std::string compiler_name;
    std::string compiler_version;
};

/// Collect environment information for the current system.
[[nodiscard]] EnvironmentInfo CollectEnvironment();

/// Get the current timestamp in ISO 8601 format.
[[nodiscard]] std::string GetTimestamp();

}  // namespace pal
}  // namespace vgcpu
