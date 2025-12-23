// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-01] CLI Frontend (Chapter 3)

#include "adapters/adapter_registry.h"
#include "assets/scene_registry.h"
#include "cli/cli_parser.h"
#include "harness/harness.h"
#include "ir/ir_loader.h"
#include "pal/environment.h"
#include "pal/timer.h"
#include "reporting/reporter.h"
#include "vgcpu/internal/log.h"
#include "vgcpu/internal/version.h"

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

#ifdef VGCPU_ENABLE_BLEND2D
#include "adapters/blend2d/blend2d_adapter.h"
#endif

#ifdef VGCPU_ENABLE_SKIA
#include "adapters/skia/skia_adapter.h"
#endif

#ifdef VGCPU_ENABLE_THORVG
#include "adapters/thorvg/thorvg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_AGG
#include "adapters/agg/agg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_QT
#include "adapters/qt/qt_adapter.h"
#endif

#ifdef VGCPU_ENABLE_AMANITHVG
#include "adapters/amanithvg/amanithvg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_RAQOTE
#include "adapters/raqote/raqote_adapter.h"
#endif

#ifdef VGCPU_ENABLE_VELLO
#include "adapters/vello/vello_adapter.h"
#endif

using namespace vgcpu;

namespace {

// Default assets directory relative to executable
const std::filesystem::path kDefaultAssetsDir = "assets/scenes";
const std::filesystem::path kDefaultManifest = "assets/scenes/manifest.json";

/// Initialize the SceneRegistry from manifest if available.
void InitSceneRegistry() {
    if (std::filesystem::exists(kDefaultManifest)) {
        auto status = SceneRegistry::Instance().LoadManifest(kDefaultManifest, kDefaultAssetsDir);
        if (status.failed()) {
            VGCPU_LOG_WARN("Failed to load scene manifest: " + status.message);
        }
    }
}

/// Handle the 'list' command.
/// Blueprint Reference: [API-01-01] CLI list subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleList(const CliOptions& options) {
    (void)options;
    std::cout << "Available Backends:\n";
    auto& registry = AdapterRegistry::Instance();
    for (const auto& id : registry.GetAdapterIds()) {
        std::cout << "  - " << id << "\n";
    }
    std::cout << "\nAvailable Scenes:\n";
    std::cout << "  - test/simple_rect (built-in test scene)\n";

    auto& scene_registry = SceneRegistry::Instance();
    for (const auto& scene_id : scene_registry.GetSceneIds()) {
        auto info = scene_registry.GetSceneInfo(scene_id);
        if (info) {
            std::cout << "  - " << scene_id;
            if (!info->description.empty()) {
                std::cout << " (" << info->description << ")";
            }
            std::cout << "\n";
        }
    }
    return 0;
}

/// Handle the 'metadata' command.
/// Blueprint Reference: [API-01-01] CLI metadata subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
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
    std::cout << "  CPU Freq:  " << (pal::GetCpuFrequency() / 1'000'000.0) << " MHz (est)\n";
    std::cout << "\nBuild Info:\n";
    std::cout << "  Version:   " << VGCPU_VERSION_STRING << "\n";
    std::cout << "  Enabled Adapters:\n";
    for (const auto& id : AdapterRegistry::Instance().GetAdapterIds()) {
        std::cout << "    - " << id << "\n";
    }
    return 0;
}

/// Handle the 'validate' command.
/// Blueprint Reference: [API-01-01] CLI validate subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleValidate(const CliOptions& options) {
    (void)options;
    if (options.validate_timer) {
        std::cout << "Validating CPU timer accuracy...\n";
        auto wall_start = pal::NowMonotonic();
        auto cpu_start = pal::GetCpuTime();

        // Busy wait for 100ms
        while (pal::Elapsed(wall_start, pal::NowMonotonic()) < std::chrono::milliseconds(100)) {
            // Spin
        }

        auto wall_end = pal::NowMonotonic();
        auto cpu_end = pal::GetCpuTime();

        auto wall_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();
        auto cpu_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(cpu_end - cpu_start).count();

        std::cout << "  Wall Time: " << wall_ms << " ms\n";
        std::cout << "  CPU Time:  " << cpu_ms << " ms\n";

        if (std::abs(wall_ms - cpu_ms) < 20) {
            std::cout << "  Status: OK (Within 20ms tolerance)\n";
        } else {
            std::cout << "  Status: WARNING (Discrepancy detected)\n";
        }
    } else {
        std::cout << "Validation not yet implemented (no manifest loaded).\n";
        std::cout << "Built-in test scene: OK\n";
    }
    return 0;
}

/// Handle the 'run' command.
/// Blueprint Reference: [API-01-01] CLI run subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
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
        VGCPU_LOG_ERROR("No backends available");
        return 1;
    }

    // Load scene(s)
    std::vector<PreparedScene> scenes;

    // Support --all-scenes to load from registry
    if (options.all_scenes) {
        auto& scene_reg = SceneRegistry::Instance();
        for (const auto& scene_id : scene_reg.GetSceneIds()) {
            auto path = scene_reg.GetScenePath(scene_id);
            if (path && std::filesystem::exists(*path)) {
                auto bytes = ir::IrLoader::LoadFromFile(*path);
                if (bytes) {
                    auto result = ir::IrLoader::Prepare(*bytes, scene_id);
                    if (result.ok()) {
                        scenes.push_back(std::move(result.value()));
                    }
                }
            }
        }
        if (!scenes.empty()) {
            VGCPU_LOG_INFO("Loaded " + std::to_string(scenes.size()) + " scenes from manifest");
        }
    } else if (!options.scenes.empty()) {
        // Load scenes from file paths or asset IDs
        for (const auto& scene_arg : options.scenes) {
            std::filesystem::path scene_path(scene_arg);

            // Check if it's a file path
            if (scene_path.extension() == ".irbin" || std::filesystem::exists(scene_path)) {
                auto bytes = ir::IrLoader::LoadFromFile(scene_path);
                if (!bytes) {
                    VGCPU_LOG_ERROR("Failed to load scene: " + std::string(scene_arg));
                    continue;
                }

                auto result = ir::IrLoader::Prepare(*bytes, scene_path.stem().string());
                if (result.failed()) {
                    VGCPU_LOG_ERROR("Failed to parse scene: " + result.status().message);
                    continue;
                }

                scenes.push_back(std::move(result.value()));
                VGCPU_LOG_INFO("Loaded scene: " + scene_arg);
            } else {
                // Try from scene registry
                auto& scene_reg = SceneRegistry::Instance();
                auto path = scene_reg.GetScenePath(scene_arg);
                if (path && std::filesystem::exists(*path)) {
                    auto bytes = ir::IrLoader::LoadFromFile(*path);
                    if (bytes) {
                        auto result = ir::IrLoader::Prepare(*bytes, scene_arg);
                        if (result.ok()) {
                            scenes.push_back(std::move(result.value()));
                            VGCPU_LOG_INFO("Loaded scene: " + scene_arg);
                        }
                    }
                } else {
                    // Try assets/scenes/<id>.irbin
                    auto asset_path =
                        std::filesystem::path("assets/scenes") / (scene_arg + ".irbin");
                    if (std::filesystem::exists(asset_path)) {
                        auto bytes = ir::IrLoader::LoadFromFile(asset_path);
                        if (bytes) {
                            auto result = ir::IrLoader::Prepare(*bytes, scene_arg);
                            if (result.ok()) {
                                scenes.push_back(std::move(result.value()));
                                VGCPU_LOG_INFO("Loaded scene: " + std::string(scene_arg));
                            }
                        }
                    } else {
                        VGCPU_LOG_WARN("Scene not found: " + std::string(scene_arg));
                    }
                }
            }
        }
    }

    // Fall back to test scene if no scenes loaded
    if (scenes.empty()) {
        scenes.push_back(ir::IrLoader::CreateTestScene(800, 600));
    }

    // Setup benchmark policy
    BenchmarkPolicy policy;
    policy.warmup_iterations = options.warmup_iters;
    policy.measurement_iterations = options.measurement_iters;
    policy.repetitions = options.repetitions;
    policy.thread_count = options.threads;
    policy.generate_png = options.generate_png;
    policy.compare_ssim = options.compare_ssim;
    policy.golden_dir = options.golden_dir;
    policy.output_dir = options.output_dir.empty() ? "." : options.output_dir;

    // Run benchmarks
    std::vector<CaseResult> results;

    for (const auto& backend_id : backend_ids) {
        auto adapter = registry.CreateAdapter(backend_id);
        if (!adapter) {
            VGCPU_LOG_WARN("Backend '" + backend_id + "' not found, skipping");
            continue;
        }

        AdapterArgs args;
        args.thread_count = policy.thread_count;
        auto status = adapter->Initialize(args);
        if (status.failed()) {
            VGCPU_LOG_WARN("Failed to initialize '" + backend_id + "': " + status.message);
            continue;
        }

        // Run each scene on this backend
        for (const auto& scene : scenes) {
            auto result = Harness::RunCase(*adapter, scene, policy);
            results.push_back(result);
        }

        adapter->Shutdown();
    }

    // Prepare metadata
    RunMetadata metadata;
    metadata.run_timestamp = pal::GetTimestamp();
    metadata.suite_version = VGCPU_VERSION_STRING;
    metadata.git_commit = VGCPU_GIT_COMMIT;
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
            VGCPU_LOG_INFO("JSON output: " + json_path.string());
        } else {
            VGCPU_LOG_ERROR("Error writing JSON: " + status.message);
        }
    }
    if (options.format == "csv" || options.format == "both") {
        auto csv_path = out_dir / "results.csv";
        auto status = CsvWriter::Write(csv_path, results);
        if (status.ok()) {
            VGCPU_LOG_INFO("CSV output: " + csv_path.string());
        } else {
            VGCPU_LOG_ERROR("Error writing CSV: " + status.message);
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

#ifdef VGCPU_ENABLE_BLEND2D
    RegisterBlend2DAdapter();
#endif

#ifdef VGCPU_ENABLE_SKIA
    RegisterSkiaAdapter();
#endif

#ifdef VGCPU_ENABLE_THORVG
    RegisterThorVGAdapter();
#endif

#ifdef VGCPU_ENABLE_AGG
    vgcpu::adapters::agg_backend::RegisterAggAdapter();
#endif

#ifdef VGCPU_ENABLE_AGG
#include "adapters/agg/agg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_QT
    RegisterQtAdapter();
#endif

#ifdef VGCPU_ENABLE_AMANITHVG
    RegisterAmanithVGAdapter();
#endif

#ifdef VGCPU_ENABLE_RAQOTE
    RegisterRaqoteAdapter();
#endif

#ifdef VGCPU_ENABLE_VELLO
    RegisterVelloAdapter();
#endif

    // Initialize scene registry from manifest
    InitSceneRegistry();

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
