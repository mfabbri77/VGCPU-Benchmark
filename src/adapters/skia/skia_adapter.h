// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include "adapters/adapter_interface.h"
#include "include/core/SkColor.h"
#include "include/core/SkPath.h"
#include "include/core/SkShader.h"
#include "ir/ir_format.h"

#include <memory>
#include <vector>

namespace vgcpu {

struct SkiaPaintObj {
    ir::PaintType type = ir::PaintType::kSolid;
    SkColor4f solid_color{0, 0, 0, 1};
    sk_sp<SkShader> shader;
};
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
    Status RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) override;

   private:
    bool initialized_ = false;
    std::vector<SkPath> prepared_paths_;
    std::vector<SkiaPaintObj> prepared_paints_;
};

void RegisterSkiaAdapter();

}  // namespace vgcpu
