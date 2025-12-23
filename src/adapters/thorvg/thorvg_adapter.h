// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] ThorVG backend
// (Chapter 4)

#pragma once

#include "adapters/adapter_interface.h"

namespace vgcpu {

/// ThorVG SW engine backend adapter.
/// Uses pure CPU software rasterization with SIMD optimization.
class ThorVGAdapter : public IBackendAdapter {
   public:
    Status Initialize(const AdapterArgs& args) override;
    Status Prepare(const PreparedScene& scene) override;
    void Shutdown() override;
    [[nodiscard]] AdapterInfo GetInfo() const override;
    [[nodiscard]] CapabilitySet GetCapabilities() const override;
    Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                  std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
};

/// Register ThorVG adapter with the adapter registry.
void RegisterThorVGAdapter();

}  // namespace vgcpu
