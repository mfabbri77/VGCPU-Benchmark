/*
 * Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adapters/adapter_interface.h"
#include "agg_path_storage.h"
#include "ir/ir_format.h"

#include <memory>
#include <vector>
namespace vgcpu::adapters::agg_backend {

struct AggPaint;

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
    Status RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
    std::vector<agg::path_storage> prepared_paths_;
    std::vector<std::shared_ptr<AggPaint>> prepared_paints_;
};

void RegisterAggAdapter();

}  // namespace vgcpu::adapters::agg_backend
