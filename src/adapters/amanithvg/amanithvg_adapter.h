// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] AmanithVG backend
// (Chapter 4)

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
    Status RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) override;

   private:
    void DestroyPaths();
    void DestroyPaints();

    bool initialized_ = false;
    void* context_ = nullptr;
    std::vector<uint32_t> vg_paths_;   // VGPath handles created in Prepare()
    std::vector<uint32_t> vg_paints_;  // VGPaint handles created in Prepare()
};

/// Register AmanithVG adapter with the adapter registry.
void RegisterAmanithVGAdapter();

}  // namespace vgcpu
