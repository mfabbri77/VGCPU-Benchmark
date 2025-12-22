// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.1 — Module cli (main entry point)

#include "adapters/adapter_registry.h"
#include "cli/cli_parser.h"
#include "harness/harness.h"
#include "ir/ir_loader.h"
#include "pal/environment.h"
#include "pal/timer.h"
#include "reporting/reporter.h"

#include <filesystem>
#include <iostream>

// Force linking of adapter implementations
#ifdef VGCPU_ENABLE_NULL_BACKEND
#include "adapters/null/null_adapter.h"
#endif

#ifdef VGCPU_ENABLE_PLUTOVG
#include "adapters/plutovg/plutovg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_CAIRO
#include "adapters/cairo/cairo_adapter.h"
#endif

using namespace vgcpu;

namespace {

/// Handle the 'list' command.
/// Blueprint Reference: Chapter 6, §6.1.1 — list subcommand
int HandleList(const CliOptions& options) {
    (void)options;
    std::cout << "Available Backends:\n";
    auto& registry = AdapterRegistry::Instance();
    for (const auto& id : registry.GetAdapterIds()) {
        std::cout << "  - " << id << "\n";
    }
    std::cout << "\nAvailable Scenes:\n";
    std::cout << "  - test/simple_rect (built-in test scene)\n";
    // TODO: Load from scene manifest
    return 0;
}

/// Handle the 'metadata' command.
/// Blueprint Reference: Chapter 6, §6.1.1 — metadata subcommand
int HandleMetadata(const CliOptions& options) {
    (void)options;
    auto env = pal::CollectEnvironment();
    std::cout << "Environment Metadata:\n";
    std::cout << "  OS:        " << env.os_name << " " << env.os_version << "\n";
    std::cout << "  Arch:      " << env.arch << "\n";
    std::cout << "  CPU:       " << env.cpu_model << "\n";
    std::cout << "  Cores:     " << env.cpu_cores << "\n";
    std::cout << "  Memory:    " << (env.memory_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "  Compiler:  " << env.compiler_name << " " << env.compiler_version << "\n";
    std::cout << "  CPU Time:  " << pal::GetCpuTimeSemantics() << "\n";
    std::cout << "\nBuild Info:\n";
    std::cout << "  Version:   0.1.0\n";
    std::cout << "  Enabled Adapters:\n";
    for (const auto& id : AdapterRegistry::Instance().GetAdapterIds()) {
        std::cout << "    - " << id << "\n";
    }
    return 0;
}

/// Handle the 'validate' command.
/// Blueprint Reference: Chapter 6, §6.1.1 — validate subcommand
int HandleValidate(const CliOptions& options) {
    (void)options;
    std::cout << "Validation not yet implemented (no manifest loaded).\n";
    std::cout << "Built-in test scene: OK\n";
    return 0;
}

/// Handle the 'run' command.
/// Blueprint Reference: Chapter 6, §6.1.1 — run subcommand
int HandleRun(const CliOptions& options) {
    auto& registry = AdapterRegistry::Instance();

    // Determine backends to use
    std::vector<std::string> backend_ids;
    if (options.all_backends || options.backends.empty()) {
        backend_ids = registry.GetAdapterIds();
    } else {
        backend_ids = options.backends;
    }

    if (backend_ids.empty()) {
        std::cerr << "Error: No backends available.\n";
        return 1;
    }

    // Create test scene (hardcoded for now)
    auto scene = ir::IrLoader::CreateTestScene(800, 600);

    // Setup benchmark policy
    BenchmarkPolicy policy;
    policy.warmup_iterations = options.warmup_iters;
    policy.measurement_iterations = options.measurement_iters;
    policy.repetitions = options.repetitions;
    policy.thread_count = options.threads;

    // Run benchmarks
    std::vector<CaseResult> results;

    for (const auto& backend_id : backend_ids) {
        auto adapter = registry.CreateAdapter(backend_id);
        if (!adapter) {
            std::cerr << "Warning: Backend '" << backend_id << "' not found, skipping.\n";
            continue;
        }

        AdapterArgs args;
        args.thread_count = policy.thread_count;
        auto status = adapter->Initialize(args);
        if (status.failed()) {
            std::cerr << "Warning: Failed to initialize '" << backend_id << "': " << status.message
                      << "\n";
            continue;
        }

        auto result = Harness::RunCase(*adapter, scene, policy);
        results.push_back(result);

        adapter->Shutdown();
    }

    // Prepare metadata
    RunMetadata metadata;
    metadata.run_timestamp = pal::GetTimestamp();
    metadata.suite_version = "0.1.0";
    metadata.git_commit = "development";
    metadata.environment = pal::CollectEnvironment();
    metadata.policy = policy;

    // Print summary
    if (options.print_summary) {
        SummaryWriter::PrintSummary(metadata, results);
    }

    // Write output files
    std::filesystem::path out_dir(options.output_dir);
    if (options.format == "json" || options.format == "both") {
        auto json_path = out_dir / "results.json";
        auto status = JsonWriter::Write(json_path, metadata, results);
        if (status.ok()) {
            std::cout << "JSON output: " << json_path << "\n";
        } else {
            std::cerr << "Error writing JSON: " << status.message << "\n";
        }
    }
    if (options.format == "csv" || options.format == "both") {
        auto csv_path = out_dir / "results.csv";
        auto status = CsvWriter::Write(csv_path, results);
        if (status.ok()) {
            std::cout << "CSV output: " << csv_path << "\n";
        } else {
            std::cerr << "Error writing CSV: " << status.message << "\n";
        }
    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Register built-in adapters
#ifdef VGCPU_ENABLE_NULL_BACKEND
    RegisterNullAdapter();
#endif

#ifdef VGCPU_ENABLE_PLUTOVG
    RegisterPlutoVGAdapter();
#endif

#ifdef VGCPU_ENABLE_CAIRO
    RegisterCairoAdapter();
#endif

    auto options = CliParser::Parse(argc, argv);
    if (!options) {
        return 1;
    }

    switch (options->command) {
        case CliCommand::kHelp:
            CliParser::PrintHelp();
            return 0;
        case CliCommand::kList:
            return HandleList(*options);
        case CliCommand::kMetadata:
            return HandleMetadata(*options);
        case CliCommand::kValidate:
            return HandleValidate(*options);
        case CliCommand::kRun:
            return HandleRun(*options);
        default:
            CliParser::PrintHelp();
            return 1;
    }
}
