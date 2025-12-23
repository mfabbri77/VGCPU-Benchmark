/*
 * Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adapters/adapter_interface.h"
#include "ir/ir_format.h"

#include <memory>
#include <vector>

namespace vgcpu::adapters::agg_backend {

class AggAdapter : public IBackendAdapter {
   public:
    AggAdapter();
    ~AggAdapter() override;

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

void RegisterAggAdapter();

}  // namespace vgcpu::adapters::agg_backend
