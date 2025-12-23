// tests/test_main.cpp
// Blueprint Reference: [TEST-01], [TASK-04.01]
// Main test runner for VGCPU-Benchmark unit tests

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Include adapter registration functions for Tier-1 backends
#include "adapters/blend2d/blend2d_adapter.h"
#include "adapters/null/null_adapter.h"
#include "adapters/plutovg/plutovg_adapter.h"

// Register adapters before tests run
// This is done via a global constructor
namespace {
struct AdapterRegistrar {
    AdapterRegistrar() {
        // Register Tier-1 backends for testing
#ifdef VGCPU_ENABLE_NULL_BACKEND
        vgcpu::RegisterNullAdapter();
#endif
#ifdef VGCPU_ENABLE_PLUTOVG
        vgcpu::RegisterPlutoVGAdapter();
#endif
#ifdef VGCPU_ENABLE_BLEND2D
        vgcpu::RegisterBlend2DAdapter();
#endif
    }
};
static AdapterRegistrar g_registrar;
}  // namespace
