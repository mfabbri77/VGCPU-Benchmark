// tests/test_main.cpp
// Blueprint Reference: [TEST-01], [TASK-04.01]
// Main test runner for VGCPU-Benchmark unit tests

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Include adapter registration functions for every backend the build
// enables. Mirrors src/cli/main.cpp's registration list exactly (ADR-0004:
// the correctness oracle census needs every compiled-in adapter visible to
// AdapterRegistry, not just Tier-1 -- registering only null/plutovg/blend2d
// here silently limited every test using AdapterRegistry::GetAdapterIds()
// to Tier-1, regardless of which backends the build actually compiled).
#include "adapters/null/null_adapter.h"

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

// Register adapters before tests run
// This is done via a global constructor
namespace {
struct AdapterRegistrar {
    AdapterRegistrar() {
#ifdef VGCPU_ENABLE_NULL_BACKEND
        vgcpu::RegisterNullAdapter();
#endif
#ifdef VGCPU_ENABLE_PLUTOVG
        vgcpu::RegisterPlutoVGAdapter();
#endif
#ifdef VGCPU_ENABLE_CAIRO
        vgcpu::RegisterCairoAdapter();
#endif
#ifdef VGCPU_ENABLE_BLEND2D
        vgcpu::RegisterBlend2DAdapter();
#endif
#ifdef VGCPU_ENABLE_SKIA
        vgcpu::RegisterSkiaAdapter();
#endif
#ifdef VGCPU_ENABLE_THORVG
        vgcpu::RegisterThorVGAdapter();
#endif
#ifdef VGCPU_ENABLE_AGG
        vgcpu::adapters::agg_backend::RegisterAggAdapter();
#endif
#ifdef VGCPU_ENABLE_QT
        vgcpu::RegisterQtAdapter();
#endif
#ifdef VGCPU_ENABLE_AMANITHVG
        vgcpu::RegisterAmanithVGAdapter();
#endif
#ifdef VGCPU_ENABLE_RAQOTE
        vgcpu::RegisterRaqoteAdapter();
#endif
#ifdef VGCPU_ENABLE_VELLO
        vgcpu::RegisterVelloAdapter();
#endif
    }
};
static AdapterRegistrar g_registrar;
}  // namespace
