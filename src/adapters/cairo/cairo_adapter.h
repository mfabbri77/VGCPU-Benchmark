// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] Cairo backend
// (Chapter 4) Blueprint Reference: backends/cairo.md

#pragma once

#include "adapters/adapter_interface.h"

namespace vgcpu {

/// Cairo backend adapter for CPU-only 2D vector rendering.
/// Uses Cairo Image Surface for pure CPU software rasterization.
class CairoAdapter : public IBackendAdapter {
   public:
    CairoAdapter() = default;
    ~CairoAdapter() override = default;

    // Lifecycle
    Status Initialize(const AdapterArgs& args) override;
    Status Prepare(const PreparedScene& scene) override;
    void Shutdown() override;

    // Metadata
    [[nodiscard]] AdapterInfo GetInfo() const override;
    [[nodiscard]] CapabilitySet GetCapabilities() const override;

    // Rendering
    Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                  std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
};

/// Register the Cairo adapter with the global registry.
void RegisterCairoAdapter();

}  // namespace vgcpu
