// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.6 — JsonWriter subcomponent

#include "reporting/reporter.h"

#include <fstream>
#include <sstream>

namespace vgcpu {

namespace {

std::string EscapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += c;
                break;
        }
    }
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

std::string JsonWriter::ToJson(const RunMetadata& metadata,
                               const std::vector<CaseResult>& results) {
    std::ostringstream oss;
    oss << "{\n";

    // Schema version
    oss << "  \"schema_version\": \"" << EscapeJson(metadata.schema_version) << "\",\n";

    // Run metadata
    oss << "  \"run_metadata\": {\n";
    oss << "    \"timestamp\": \"" << EscapeJson(metadata.run_timestamp) << "\",\n";
    oss << "    \"suite_version\": \"" << EscapeJson(metadata.suite_version) << "\",\n";
    oss << "    \"git_commit\": \"" << EscapeJson(metadata.git_commit) << "\",\n";
    oss << "    \"environment\": {\n";
    oss << "      \"os_name\": \"" << EscapeJson(metadata.environment.os_name) << "\",\n";
    oss << "      \"os_version\": \"" << EscapeJson(metadata.environment.os_version) << "\",\n";
    oss << "      \"arch\": \"" << EscapeJson(metadata.environment.arch) << "\",\n";
    oss << "      \"cpu_model\": \"" << EscapeJson(metadata.environment.cpu_model) << "\",\n";
    oss << "      \"cpu_cores\": " << metadata.environment.cpu_cores << ",\n";
    oss << "      \"memory_bytes\": " << metadata.environment.memory_bytes << ",\n";
    oss << "      \"compiler_name\": \"" << EscapeJson(metadata.environment.compiler_name)
        << "\",\n";
    oss << "      \"compiler_version\": \"" << EscapeJson(metadata.environment.compiler_version)
        << "\"\n";
    oss << "    },\n";
    oss << "    \"policy\": {\n";
    oss << "      \"warmup_iterations\": " << metadata.policy.warmup_iterations << ",\n";
    oss << "      \"measurement_iterations\": " << metadata.policy.measurement_iterations << ",\n";
    oss << "      \"repetitions\": " << metadata.policy.repetitions << ",\n";
    oss << "      \"thread_count\": " << metadata.policy.thread_count << "\n";
    oss << "    }\n";
    oss << "  },\n";

    // Cases
    oss << "  \"cases\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        oss << "    {\n";
        oss << "      \"backend_id\": \"" << EscapeJson(r.backend_id) << "\",\n";
        oss << "      \"scene_id\": \"" << EscapeJson(r.scene_id) << "\",\n";
        oss << "      \"scene_hash\": \"" << EscapeJson(r.scene_hash) << "\",\n";
        oss << "      \"width\": " << r.width << ",\n";
        oss << "      \"height\": " << r.height << ",\n";
        oss << "      \"decision\": \"" << DecisionToString(r.decision) << "\",\n";
        oss << "      \"reasons\": [";
        for (size_t j = 0; j < r.reasons.size(); ++j) {
            oss << "\"" << EscapeJson(r.reasons[j]) << "\"";
            if (j + 1 < r.reasons.size())
                oss << ", ";
        }
        oss << "],\n";
        oss << "      \"stats\": {\n";
        oss << "        \"wall_p50_ns\": " << r.stats.wall_p50_ns << ",\n";
        oss << "        \"wall_p90_ns\": " << r.stats.wall_p90_ns << ",\n";
        oss << "        \"cpu_p50_ns\": " << r.stats.cpu_p50_ns << ",\n";
        oss << "        \"cpu_p90_ns\": " << r.stats.cpu_p90_ns << ",\n";
        oss << "        \"sample_count\": " << r.stats.sample_count << "\n";
        oss << "      }\n";
        oss << "    }";
        if (i + 1 < results.size())
            oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";

    return oss.str();
}

Status JsonWriter::Write(const std::filesystem::path& path, const RunMetadata& metadata,
                         const std::vector<CaseResult>& results) {
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

    file << ToJson(metadata, results);
    if (!file) {
        return Status::IOError("Failed to write to file: " + path.string());
    }

    return Status::Ok();
}

}  // namespace vgcpu
