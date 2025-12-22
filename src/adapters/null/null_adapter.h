// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.5 — Null/Debug backend for testing

#pragma once

#include "adapters/adapter_interface.h"

namespace vgcpu {

/// Null backend adapter for testing the harness.
/// This adapter performs no actual rendering but implements
/// the full IBackendAdapter interface for harness testing.
class NullAdapter : public IBackendAdapter {
   public:
    NullAdapter() = default;
    ~NullAdapter() override = default;

    // Lifecycle
    Status Initialize(const AdapterArgs& args) override;
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

/// Register the Null adapter with the global registry.
/// Must be called before using the Null backend.
void RegisterNullAdapter();

}  // namespace vgcpu
