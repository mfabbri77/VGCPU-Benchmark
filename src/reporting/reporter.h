// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-10] Reporting (Chapter 3) / [API-06-07] Reporting: CSV/JSON emit
// (Chapter 4)

#pragma once

#include "common/status.h"
#include "harness/harness.h"
#include "pal/environment.h"
#include "vgcpu/internal/version.h"

#include <filesystem>
#include <string>

namespace vgcpu {

/// Run metadata for reporting.
/// Blueprint Reference: [API-01-02] Report schemas (Chapter 4) / [API-06-06] RunReport (Chapter 4)
struct RunMetadata {
    std::string schema_version = VGCPU_REPORT_SCHEMA_VERSION;
    std::string run_timestamp;
    std::string suite_version;
    std::string git_commit;
    pal::EnvironmentInfo environment;
    BenchmarkPolicy policy;
};

/// JSON result writer.
/// Blueprint Reference: [REQ-48] JSON output format (Chapter 4) / [API-06-07] WriteJson (Chapter 4)
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
/// Blueprint Reference: [REQ-48] CSV output format (Chapter 4) / [API-06-07] WriteCsv (Chapter 4)
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
/// Blueprint Reference: [API-06-07] WriteSummaryToStdout (Chapter 4) / [ARCH-10-10] (Chapter 3)
class SummaryWriter {
   public:
    /// Write a human-readable summary to stdout.
    /// @param metadata Run metadata.
    /// @param results Benchmark case results.
    static void PrintSummary(const RunMetadata& metadata, const std::vector<CaseResult>& results);
};

}  // namespace vgcpu
