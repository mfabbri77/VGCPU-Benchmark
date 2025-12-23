// tests/test_hotpath.cpp
// Blueprint Reference: [REQ-113], [TASK-04.03]
// Unit tests for rendering hot-path allocation enforcement

#include "adapters/adapter_registry.h"
#include "doctest.h"
#include "ir/ir_loader.h"
#include "vgcpu/internal/alloc_tracker.h"

namespace vgcpu {

TEST_SUITE("Hot-Path Allocation Enforcement") {
    TEST_CASE("NullAdapter::Render performs zero dynamic allocations" *
              doctest::test_suite("hotpath")) {
#ifdef VGCPU_ENABLE_ALLOC_INSTRUMENTATION
        auto& registry = AdapterRegistry::Instance();
        auto loader = ir::IrLoader::CreateTestScene(200, 200);
        SurfaceConfig config;
        config.width = 200;
        config.height = 200;
        std::vector<uint8_t> buffer(config.width * config.height * 4, 0);

        auto backend_ids = registry.GetAdapterIds();
        for (const auto& id : backend_ids) {
            auto adapter = registry.CreateAdapter(id);
            REQUIRE(adapter != nullptr);

            CAPTURE(id);

            // Warmup/Init (allocations allowed here)
            AdapterArgs args;
            REQUIRE(adapter->Initialize(args).ok());
            REQUIRE(adapter->Prepare(loader).ok());

            // Hot Path starts here
            {
                internal::ScopedAllocationGuard guard;
                auto status = adapter->Render(loader, config, buffer);
                REQUIRE(status.ok());

                size_t allocs = guard.GetAllocationCount();
                CAPTURE(allocs);
                if (id == "null") {
                    CHECK_MESSAGE(
                        allocs == 0,
                        "Detected " << allocs << " allocations in NullAdapter::Render hot-path");
                } else if (allocs > 0) {
                    MESSAGE("Backend " << id << " performed " << allocs
                                       << " allocations in Render");
                }
            }

            adapter->Shutdown();
        }
#else
        MESSAGE("Allocation instrumentation disabled - skipping test");
#endif
    }
}

}  // namespace vgcpu
