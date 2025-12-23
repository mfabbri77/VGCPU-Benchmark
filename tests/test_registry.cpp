// tests/test_registry.cpp
// Blueprint Reference: [TEST-10], [TEST-12], [TASK-05.02]
// Unit tests for Backend Adapter Registry

#include "adapters/adapter_registry.h"
#include "doctest.h"

TEST_SUITE("Adapter Registry") {
    TEST_CASE("Registry returns non-empty list of backends" * doctest::test_suite("registry")) {
        // [TEST-10] registry_contains_tier1
        auto& registry = vgcpu::AdapterRegistry::Instance();
        auto backends = registry.GetAdapterIds();

        CHECK(!backends.empty());
    }

    TEST_CASE("Registry contains null backend (Tier-1)" * doctest::test_suite("registry")) {
        // [TEST-10] Null backend must always be present as Tier-1
        auto& registry = vgcpu::AdapterRegistry::Instance();

        CHECK(registry.HasAdapter("null"));
    }

    TEST_CASE("CreateAdapter returns valid adapter for null backend" *
              doctest::test_suite("registry")) {
        // [TEST-12] Verify adapter creation
        auto& registry = vgcpu::AdapterRegistry::Instance();
        auto adapter = registry.CreateAdapter("null");

        REQUIRE(adapter != nullptr);
        CHECK(adapter->GetInfo().id == "null");
    }

    TEST_CASE("CreateAdapter returns nullptr for unknown backend" *
              doctest::test_suite("registry")) {
        // [TEST-13] Unknown backend handling
        auto& registry = vgcpu::AdapterRegistry::Instance();
        auto adapter = registry.CreateAdapter("nonexistent_backend_xyz");

        CHECK(adapter == nullptr);
    }

    TEST_CASE("All listed backends can be created" * doctest::test_suite("registry")) {
        // [TEST-14] Verify all backends are creatable
        auto& registry = vgcpu::AdapterRegistry::Instance();
        auto backend_ids = registry.GetAdapterIds();

        for (const auto& id : backend_ids) {
            auto adapter = registry.CreateAdapter(id);
            CAPTURE(id);
            CHECK(adapter != nullptr);
        }
    }

    TEST_CASE("Adapter info has required fields" * doctest::test_suite("registry")) {
        // [TEST-15] Verify adapter metadata
        auto& registry = vgcpu::AdapterRegistry::Instance();
        auto adapter = registry.CreateAdapter("null");

        REQUIRE(adapter != nullptr);
        auto info = adapter->GetInfo();

        CHECK(!info.id.empty());
        CHECK(!info.detailed_name.empty());
    }
}
