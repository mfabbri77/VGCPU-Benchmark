// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: backends/amanithvg.md

#pragma once

#include "adapters/adapter_interface.h"

namespace vgcpu {

/// AmanithVG SRE (Software Rendering Engine) backend adapter.
/// Uses OpenVG 1.1 API with Mazatech SRE extensions for CPU-only rendering.
class AmanithVGAdapter : public IBackendAdapter {
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

/// Register AmanithVG adapter with the adapter registry.
void RegisterAmanithVGAdapter();

}  // namespace vgcpu
