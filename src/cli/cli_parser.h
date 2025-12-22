// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.1 — Module cli

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace vgcpu {

/// CLI subcommand types.
/// Blueprint Reference: Chapter 6, §6.1.1 — CLI Command Model
enum class CliCommand {
    kNone,
    kHelp,
    kRun,
    kList,
    kMetadata,
    kValidate,
};

/// Parsed CLI options.
/// Blueprint Reference: Chapter 6, §6.1.2 — CLI Options
struct CliOptions {
    CliCommand command = CliCommand::kNone;

    // Selection
    std::vector<std::string> backends;
    std::vector<std::string> scenes;
    bool all_backends = false;
    bool all_scenes = false;

    // Benchmark policy
    int warmup_iters = 3;
    int measurement_iters = 10;
    int repetitions = 1;
    int threads = 1;

    // Output
    std::string output_dir = ".";
    std::string format = "json";  // json, csv, both
    bool print_summary = true;

    // Flags
    bool fail_fast = false;
    bool help = false;
};

/// CLI argument parser.
/// Blueprint Reference: Chapter 7, §7.2.1 — cli module responsibilities
class CliParser {
   public:
    /// Parse command-line arguments.
    /// @param argc Argument count.
    /// @param argv Argument values.
    /// @return Parsed options, or nullopt on error.
    static std::optional<CliOptions> Parse(int argc, char* argv[]);

    /// Print help message.
    static void PrintHelp();

    /// Print version.
    static void PrintVersion();
};

}  // namespace vgcpu
