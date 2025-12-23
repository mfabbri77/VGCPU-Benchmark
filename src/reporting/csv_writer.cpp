// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-10] Reporting (Chapter 3) / [API-06-07] Reporting: CSV/JSON emit
// (Chapter 4)

#include "reporting/reporter.h"
#include "vgcpu/internal/version.h"

#include <fstream>
#include <sstream>

namespace vgcpu {

namespace {

std::string EscapeCsv(const std::string& s) {
    // If the string contains comma, quote, or newline, quote it
    bool needs_quote = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quote = true;
            break;
        }
    }

    if (!needs_quote) {
        return s;
    }

    std::string result = "\"";
    for (char c : s) {
        if (c == '"') {
            result += "\"\"";  // Escape double quotes
        } else {
            result += c;
        }
    }
    result += "\"";
    return result;
}

std::string DecisionToString(CaseDecision decision) {
    switch (decision) {
        case CaseDecision::kExecute:
            return "EXECUTE";
        case CaseDecision::kSkip:
            return "SKIP";
        case CaseDecision::kFail:
            return "FAIL";
        case CaseDecision::kFallback:
            return "FALLBACK";
    }
    return "UNKNOWN";
}

}  // namespace

std::string CsvWriter::ToCsv(const std::vector<CaseResult>& results) {
    std::ostringstream oss;

    // Schema version header per [REQ-133-02]
    oss << "# schema_version=" << VGCPU_REPORT_SCHEMA_VERSION << "\n";

    // Header row
    // Blueprint Reference: [REQ-49] Report MUST carry tool_version/schema_version (Chapter 4)
    oss << "backend_id,scene_id,scene_hash,width,height,decision,";
    oss << "wall_p50_ns,wall_p90_ns,cpu_p50_ns,cpu_p90_ns,sample_count\n";

    // Data rows
    for (const auto& r : results) {
        oss << EscapeCsv(r.backend_id) << ",";
        oss << EscapeCsv(r.scene_id) << ",";
        oss << EscapeCsv(r.scene_hash) << ",";
        oss << r.width << ",";
        oss << r.height << ",";
        oss << DecisionToString(r.decision) << ",";
        oss << r.stats.wall_p50_ns << ",";
        oss << r.stats.wall_p90_ns << ",";
        oss << r.stats.cpu_p50_ns << ",";
        oss << r.stats.cpu_p90_ns << ",";
        oss << r.stats.sample_count << "\n";
    }

    return oss.str();
}

Status CsvWriter::Write(const std::filesystem::path& path, const std::vector<CaseResult>& results) {
    // Ensure parent directory exists
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return Status::IOError("Failed to create directory: " + path.parent_path().string() +
                                   " (" + ec.message() + ")");
        }
    }

    std::ofstream file(path);
    if (!file) {
        return Status::IOError("Failed to open file for writing: " + path.string());
    }

    file << ToCsv(results);
    if (!file) {
        return Status::IOError("Failed to write to file: " + path.string());
    }

    return Status::Ok();
}

}  // namespace vgcpu
