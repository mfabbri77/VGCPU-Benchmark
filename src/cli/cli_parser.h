// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-01] CLI Frontend (Chapter 3)

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace vgcpu {

/// CLI subcommand types.
/// Blueprint Reference: [API-01-01] CLI contract (Chapter 4) / [ARCH-13-01] (Chapter 3)
enum class CliCommand {
    kNone,
    kHelp,
    kRun,
    kList,
    kMetadata,
    kValidate,
};

/// Parsed CLI options.
/// Blueprint Reference: [REQ-46] CLI behavior (Chapter 4) / [ARCH-14-A] (Chapter 3)
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
    bool validate_timer = false;
};

/// CLI argument parser.
/// Blueprint Reference: [ARCH-10-01] CLI Responsibilities (Chapter 3)
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
