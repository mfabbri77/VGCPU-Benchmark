// tests/test_concurrency.cpp
// Blueprint Reference: [TEST-14], [TASK-04.04]
// Unit tests for thread-safety and idempotency of Render()

#include "adapters/adapter_registry.h"
#include "doctest.h"
#include "ir/ir_loader.h"

#include <atomic>
#include <thread>
#include <vector>

namespace vgcpu {

TEST_SUITE("Concurrency & Idempotency") {
    TEST_CASE("Parallel-capable adapters produce bit-identical results" *
              doctest::test_suite("concurrency")) {
        auto& registry = AdapterRegistry::Instance();
        auto scene = ir::IrLoader::CreateTestScene(200, 200);
        SurfaceConfig config;
        config.width = 200;
        config.height = 200;

        auto backend_ids = registry.GetAdapterIds();
        for (const auto& id : backend_ids) {
            auto adapter = registry.CreateAdapter(id);
            REQUIRE(adapter != nullptr);

            auto caps = adapter->GetCapabilities();
            if (!caps.supports_parallel_render) {
                continue;  // Skip adapters that don't support parallel render
            }

            CAPTURE(id);

            // Initialize adapter
            AdapterArgs args;
            args.thread_count = 1;  // Backend internal threads (not our test threads)
            REQUIRE(adapter->Initialize(args).ok());
            REQUIRE(adapter->Prepare(scene).ok());

            // Run concurrent rendering
            const int kThreadCount = 4;
            std::vector<std::vector<uint8_t>> buffers(kThreadCount);
            std::vector<std::thread> threads;
            std::atomic<bool> success{true};
            std::string error_msg;

            for (int i = 0; i < kThreadCount; ++i) {
                buffers[i].resize(config.width * config.height * 4, 0);
                threads.emplace_back(
                    [&adapter, &scene, &config, &buffer = buffers[i], &success, &error_msg]() {
                        auto status = adapter->Render(scene, config, buffer);
                        if (status.failed()) {
                            success = false;
                            error_msg = status.message;
                        }
                    });
            }

            for (auto& t : threads) {
                t.join();
            }

            if (!success) {
                INFO("Render failed: " << error_msg);
                REQUIRE(success);
            }

            // Verify idempotency (all buffers must be identical)
            for (int i = 1; i < kThreadCount; ++i) {
                if (buffers[0] != buffers[i]) {
                    INFO("Divergence detected in thread " << i << " for backend " << id);
                    CHECK(buffers[0] == buffers[i]);
                }
            }

            adapter->Shutdown();
        }
    }
}

}  // namespace vgcpu
