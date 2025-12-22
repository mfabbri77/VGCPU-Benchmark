// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.6 — CsvWriter subcomponent

#include "reporting/reporter.h"

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

    // Header row
    // Blueprint Reference: Chapter 6, §6.6.2 — CSV columns
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
