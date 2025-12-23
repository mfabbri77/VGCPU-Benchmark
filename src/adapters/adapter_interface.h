// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] IBackendAdapter
// (Chapter 4)

#pragma once

#include "common/capability_set.h"
#include "common/status.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vgcpu {

// Forward declarations
struct PreparedScene;

/// Adapter metadata information.
/// Blueprint Reference: [API-06-05] BackendDescriptor (Chapter 4)
struct AdapterInfo {
    std::string id;             ///< Stable identifier (e.g., "cairo_image")
    std::string detailed_name;  ///< Human-readable name (e.g., "Cairo Image Surface")
    std::string version;        ///< Library version string
    bool is_cpu_only = true;    ///< CPU-only enforcement flag
};

/// Surface configuration for rendering.
/// Blueprint Reference: [API-06-05] SurfaceDesc (Chapter 4)
struct SurfaceConfig {
    int width = 0;
    int height = 0;
    // Future: pixel format, premultiplication settings, etc.
};

/// Initialization arguments for adapters.
struct AdapterArgs {
    int thread_count = 1;  ///< Thread count hint (0 = use backend default)
    // Future: other configuration options
};

/// Abstract interface for backend adapters.
/// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] IBackendAdapter
/// (Chapter 4)
class IBackendAdapter {
   public:
    virtual ~IBackendAdapter() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Initialize the backend with the given arguments.
    /// Called once before any rendering operations.
    virtual Status Initialize(const AdapterArgs& args) = 0;

    /// Prepare a scene for rendering.
    /// Called once per scene before any measurements begin. [ARCH-14-F]
    /// This is where backends should compile shaders, upload textures, etc.
    virtual Status Prepare(const PreparedScene& scene) = 0;

    /// Shutdown the backend and release resources.
    /// Called once after all rendering is complete.
    virtual void Shutdown() = 0;

    // -------------------------------------------------------------------------
    // Metadata
    // -------------------------------------------------------------------------

    /// Get adapter identification and metadata.
    [[nodiscard]] virtual AdapterInfo GetInfo() const = 0;

    /// Get the capability set for this backend.
    [[nodiscard]] virtual CapabilitySet GetCapabilities() const = 0;

    // -------------------------------------------------------------------------
    // Rendering
    // Blueprint Reference: [API-04] Thread-safety and reentrancy (Chapter 4) / [REQ-56] Reentrancy
    // rules (Chapter 4) Blueprint Reference: [REQ-56-02] CPU-Only Enforcement (Chapter 4)
    // -------------------------------------------------------------------------

    /// Render the scene to an output buffer (hot path). [ARCH-14-F]
    /// @param scene The prepared scene to render.
    /// @param config Surface configuration (width, height).
    /// @param output_buffer Output pixel buffer (RGBA8, premultiplied).
    ///                      Will be resized to width * height * 4 bytes.
    /// @return Status indicating success or failure.
    virtual Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                          std::vector<uint8_t>& output_buffer) = 0;
};

}  // namespace vgcpu
