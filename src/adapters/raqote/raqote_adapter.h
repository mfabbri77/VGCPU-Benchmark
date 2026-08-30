// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include "adapters/adapter_registry.h"

extern "C" {
struct RqtPathBuf;
}

#include <memory>
#include <vector>
namespace vgcpu {

class RaqoteAdapter : public IBackendAdapter {
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
    std::vector<RqtPathBuf*> prepared_paths_;
};
void RegisterRaqoteAdapter();

}  // namespace vgcpu
