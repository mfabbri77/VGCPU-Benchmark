// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.6 — Module reporting

#pragma once

#include "common/status.h"
#include "harness/harness.h"
#include "pal/environment.h"

#include <filesystem>
#include <string>

namespace vgcpu {

/// Run metadata for reporting.
/// Blueprint Reference: Chapter 6, §6.6.1 — run_metadata
struct RunMetadata {
    std::string schema_version = "0.1.0";
    std::string run_timestamp;
    std::string suite_version;
    std::string git_commit;
    pal::EnvironmentInfo environment;
    BenchmarkPolicy policy;
};

/// JSON result writer.
/// Blueprint Reference: Chapter 6, §6.6.1 — Machine-Readable Output (JSON)
class JsonWriter {
   public:
    /// Write run results to a JSON file.
    /// @param path Output file path.
    /// @param metadata Run metadata.
    /// @param results Benchmark case results.
    /// @return Status indicating success or failure.
    static Status Write(const std::filesystem::path& path, const RunMetadata& metadata,
                        const std::vector<CaseResult>& results);

    /// Write run results to a JSON string.
    static std::string ToJson(const RunMetadata& metadata, const std::vector<CaseResult>& results);
};

/// CSV result writer.
/// Blueprint Reference: Chapter 6, §6.6.2 — Machine-Readable Output (CSV)
class CsvWriter {
   public:
    /// Write run results to a CSV file.
    /// @param path Output file path.
    /// @param results Benchmark case results.
    /// @return Status indicating success or failure.
    static Status Write(const std::filesystem::path& path, const std::vector<CaseResult>& results);

    /// Write run results to a CSV string.
    static std::string ToCsv(const std::vector<CaseResult>& results);
};

/// Human-readable summary writer.
/// Blueprint Reference: Chapter 6, §6.1.2 — summary format
class SummaryWriter {
   public:
    /// Write a human-readable summary to stdout.
    /// @param metadata Run metadata.
    /// @param results Benchmark case results.
    static void PrintSummary(const RunMetadata& metadata, const std::vector<CaseResult>& results);
};

}  // namespace vgcpu
