// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.5 — Null/Debug backend for testing

#include "adapters/null/null_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/prepared_scene.h"

namespace vgcpu {

Status NullAdapter::Initialize(const AdapterArgs& args) {
    (void)args;  // Null adapter ignores arguments
    initialized_ = true;
    return Status::Ok();
}

Status NullAdapter::Prepare(const PreparedScene& scene) {
    (void)scene;
    if (!initialized_) {
        return Status::Fail("NullAdapter not initialized");
    }
    return Status::Ok();
}

void NullAdapter::Shutdown() {
    initialized_ = false;
}

AdapterInfo NullAdapter::GetInfo() const {
    return AdapterInfo{.id = "null",
                       .detailed_name = "Null Backend (Debug/Testing)",
                       .version = "1.0.0",
                       .is_cpu_only = true};
}

CapabilitySet NullAdapter::GetCapabilities() const {
    // Null backend claims to support everything
    return CapabilitySet::All();
}

Status NullAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) {
    (void)output_buffer;  // Null adapter does not write to buffer
    if (!initialized_) {
        return Status::Fail("NullAdapter not initialized");
    }

    if (!scene.IsValid()) {
        return Status::InvalidArg("Invalid scene");
    }

    if (config.width <= 0 || config.height <= 0) {
        return Status::InvalidArg("Invalid surface configuration");
    }

    // Buffer is pre-sized by harness. Contents are undefined until kClear.
    // For null backend, we do nothing - just return success immediately.

    return Status::Ok();
}

// Explicit registration function (called from main)
void RegisterNullAdapter() {
    AdapterRegistry::Instance().Register("null", "Null Backend (Debug/Testing)",
                                         []() { return std::make_unique<NullAdapter>(); });
}

}  // namespace vgcpu
