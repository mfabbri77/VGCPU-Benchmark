// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <vector>

#include "adapters/adapter_interface.h"

extern "C" {
struct VloPath;
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

    bool initialized_ = false;
    std::vector<VloPath*> prepared_paths_;
};
void RegisterVelloAdapter();

}  // namespace vgcpu
