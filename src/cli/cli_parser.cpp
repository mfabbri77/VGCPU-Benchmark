// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.1 — Module cli

#include "cli/cli_parser.h"

#include <cstring>
#include <iostream>
#include <sstream>

namespace vgcpu {

namespace {

std::vector<std::string> SplitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

}  // namespace

void CliParser::PrintVersion() {
    std::cout << "vgcpu-benchmark v0.1.0\n";
    std::cout << "CPU-only 2D Vector Graphics Benchmark Suite\n";
    std::cout << "Copyright (c) 2025 Michele Fabbri\n";
}

void CliParser::PrintHelp() {
    PrintVersion();
    std::cout << "\nUsage: vgcpu-benchmark <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  run        Execute benchmarks\n";
    std::cout << "  list       List available backends and scenes\n";
    std::cout << "  metadata   Print environment and build metadata\n";
    std::cout << "  validate   Validate scene manifest and IR assets\n";
    std::cout << "\nRun Options:\n";
    std::cout << "  --backend <id,...>     Select backends (comma-separated)\n";
    std::cout << "  --scene <id,...>       Select scenes (comma-separated)\n";
    std::cout << "  --all-backends         Include all available backends\n";
    std::cout << "  --all-scenes           Include all available scenes\n";
    std::cout << "  --warmup-iters <n>     Warmup iterations (default: 3)\n";
    std::cout << "  --iters <n>            Measurement iterations (default: 10)\n";
    std::cout << "  --repetitions <n>      Run repetitions (default: 1)\n";
    std::cout << "  --threads <n>          Thread count (default: 1)\n";
    std::cout << "  --out <path>           Output directory (default: .)\n";
    std::cout << "  --format <type>        Output format: json, csv, both (default: json)\n";
    std::cout << "  --fail-fast            Stop on first failure\n";
    std::cout << "\nGeneral Options:\n";
    std::cout << "  --help, -h             Print this help message\n";
    std::cout << "  --version, -v          Print version\n";
}

std::optional<CliOptions> CliParser::Parse(int argc, char* argv[]) {
    CliOptions options;

    if (argc < 2) {
        options.command = CliCommand::kHelp;
        return options;
    }

    // Parse command
    std::string cmd = argv[1];
    if (cmd == "run") {
        options.command = CliCommand::kRun;
    } else if (cmd == "list") {
        options.command = CliCommand::kList;
    } else if (cmd == "metadata") {
        options.command = CliCommand::kMetadata;
    } else if (cmd == "validate") {
        options.command = CliCommand::kValidate;
    } else if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        options.command = CliCommand::kHelp;
        return options;
    } else if (cmd == "--version" || cmd == "-v") {
        PrintVersion();
        return std::nullopt;  // Exit after printing version
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << "Use 'vgcpu-benchmark --help' for usage.\n";
        return std::nullopt;
    }

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--backend" && i + 1 < argc) {
            options.backends = SplitString(argv[++i], ',');
        } else if (arg == "--scene" && i + 1 < argc) {
            options.scenes = SplitString(argv[++i], ',');
        } else if (arg == "--all-backends") {
            options.all_backends = true;
        } else if (arg == "--all-scenes") {
            options.all_scenes = true;
        } else if (arg == "--warmup-iters" && i + 1 < argc) {
            options.warmup_iters = std::stoi(argv[++i]);
        } else if (arg == "--iters" && i + 1 < argc) {
            options.measurement_iters = std::stoi(argv[++i]);
        } else if (arg == "--repetitions" && i + 1 < argc) {
            options.repetitions = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            options.threads = std::stoi(argv[++i]);
        } else if (arg == "--out" && i + 1 < argc) {
            options.output_dir = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            options.format = argv[++i];
        } else if (arg == "--fail-fast") {
            options.fail_fast = true;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return std::nullopt;
        }
    }

    return options;
}

}  // namespace vgcpu
