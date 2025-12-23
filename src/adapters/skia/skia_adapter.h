// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include "adapters/adapter_interface.h"

namespace vgcpu {

class SkiaAdapter : public IBackendAdapter {
   public:
    SkiaAdapter() = default;
    ~SkiaAdapter() override = default;

    Status Initialize(const AdapterArgs& args) override;
    Status Prepare(const PreparedScene& scene) override;
    void Shutdown() override;
    AdapterInfo GetInfo() const override;
    CapabilitySet GetCapabilities() const override;

    Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                  std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
};

void RegisterSkiaAdapter();

}  // namespace vgcpu
