// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: backends/blend2d.md

#pragma once

#include "adapters/adapter_interface.h"

#include <blend2d/blend2d.h>

namespace vgcpu {

/// Blend2D backend adapter implementation.
class Blend2DAdapter : public IBackendAdapter {
   public:
    Blend2DAdapter() = default;
    ~Blend2DAdapter() override = default;

    // Lifecycle
    Status Initialize(const AdapterArgs& args) override;
    void Shutdown() override;

    // Metadata
    [[nodiscard]] AdapterInfo GetInfo() const override;
    [[nodiscard]] CapabilitySet GetCapabilities() const override;

    // Rendering
    Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                  std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
    uint32_t thread_count_ = 1;
};

/// Register the Blend2D adapter with the global registry.
void RegisterBlend2DAdapter();

}  // namespace vgcpu
