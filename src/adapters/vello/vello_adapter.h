// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include "adapters/adapter_interface.h"

#include <concepts>
#include <memory>
#include <string>
#include <vector>

extern "C" {
struct VloPath;
struct VloPaintBuf;
}
namespace vgcpu {

class VelloAdapter : public IBackendAdapter {
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
    std::vector<VloPath*> prepared_paths_;
    std::vector<VloPaintBuf*> prepared_paints_;
};
void RegisterVelloAdapter();

}  // namespace vgcpu
