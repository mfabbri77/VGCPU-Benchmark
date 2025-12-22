// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 6, §6.3.2 — PreparedScene Contract

#pragma once

#include "ir/ir_format.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vgcpu {

/// A single path geometry.
/// Blueprint Reference: Chapter 5, §5.3.4 — Path Data Block Format
struct Path {
    std::vector<ir::PathVerb> verbs;
    std::vector<float> points;  ///< x, y pairs
};

/// A paint definition (solid color or gradient).
/// Blueprint Reference: Chapter 5, §5.3.5 — Paint Entry Format
struct Paint {
    ir::PaintType type = ir::PaintType::kSolid;

    // Solid color
    uint32_t color = 0xFF000000;  ///< RGBA8 premultiplied (default: opaque black)

    // Linear gradient
    float linear_start_x = 0.0f;
    float linear_start_y = 0.0f;
    float linear_end_x = 0.0f;
    float linear_end_y = 0.0f;

    // Radial gradient
    float radial_center_x = 0.0f;
    float radial_center_y = 0.0f;
    float radial_radius = 0.0f;

    // Gradient stops (shared by linear and radial)
    std::vector<ir::GradientStop> stops;
};

/// Immutable prepared scene optimized for replay.
/// Blueprint Reference: Chapter 6, §6.3.2 — PreparedScene Contract
struct PreparedScene {
    // Header info
    uint32_t width = 0;
    uint32_t height = 0;

    // Scene identification
    std::string scene_id;
    std::string scene_hash;  ///< SHA-256 hex digest
    uint8_t ir_major_version = 0;
    uint8_t ir_minor_version = 0;

    // Resource tables
    std::vector<Paint> paints;
    std::vector<Path> paths;

    // Command stream (raw bytes for adapter iteration)
    std::vector<uint8_t> command_stream;

    /// Check if the scene is valid and ready for rendering.
    [[nodiscard]] bool IsValid() const {
        return width > 0 && height > 0 && !command_stream.empty();
    }
};

}  // namespace vgcpu
