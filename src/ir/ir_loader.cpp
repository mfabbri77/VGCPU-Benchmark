// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 6, §6.3.1 — IR Runtime API

#include "ir/ir_loader.h"

#include <cstring>
#include <fstream>

namespace vgcpu {
namespace ir {

std::optional<std::vector<uint8_t>> IrLoader::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::nullopt;
    }

    auto size = file.tellg();
    if (size <= 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::nullopt;
    }

    return buffer;
}

ValidationReport IrLoader::Validate(const std::vector<uint8_t>& bytes) {
    ValidationReport report;
    report.valid = true;

    // Check minimum size for header
    if (bytes.size() < sizeof(IrHeader)) {
        report.valid = false;
        report.errors.push_back("File too small: missing IR header");
        return report;
    }

    // Check magic bytes
    if (bytes[0] != 'V' || bytes[1] != 'G' || bytes[2] != 'I' || bytes[3] != 'R') {
        report.valid = false;
        report.errors.push_back("Invalid magic bytes: expected 'VGIR'");
        return report;
    }

    // Check major version
    uint8_t major = bytes[4];
    if (major != kIrMajorVersion) {
        report.valid = false;
        report.errors.push_back("Unsupported IR major version: " + std::to_string(major) +
                                " (expected: " + std::to_string(kIrMajorVersion) + ")");
        return report;
    }

    // Check total size consistency
    IrHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.total_size != bytes.size()) {
        report.valid = false;
        report.errors.push_back("Size mismatch: header says " + std::to_string(header.total_size) +
                                " but file is " + std::to_string(bytes.size()) + " bytes");
        return report;
    }

    // TODO: Validate CRC32
    // TODO: Validate sections structure
    // TODO: Validate command stream references

    return report;
}

Result<PreparedScene> IrLoader::Prepare(const std::vector<uint8_t>& bytes,
                                        const std::string& scene_id) {
    // Validate first
    auto report = Validate(bytes);
    if (!report.valid) {
        std::string errors;
        for (const auto& e : report.errors) {
            if (!errors.empty())
                errors += "; ";
            errors += e;
        }
        return Status::Fail("IR validation failed: " + errors);
    }

    PreparedScene scene;
    scene.scene_id = scene_id;
    scene.scene_hash = ComputeHash(bytes);

    // Parse header
    IrHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    scene.ir_major_version = header.major_ver;
    scene.ir_minor_version = header.minor_ver;

    // TODO: Parse sections (Paint, Path, Command)
    // For now, return a minimal valid scene for testing
    scene.width = 800;
    scene.height = 600;

    // Create a simple command stream: just Clear and End
    scene.command_stream = {
        static_cast<uint8_t>(Opcode::kClear), 0xFF, 0xFF, 0xFF, 0xFF,  // White background (RGBA)
        static_cast<uint8_t>(Opcode::kEnd)};

    return scene;
}

std::string IrLoader::ComputeHash(const std::vector<uint8_t>& bytes) {
    // TODO: Implement SHA-256 hashing
    // For now, return a placeholder hash based on size
    // Blueprint Reference: Chapter 5, §5.4.1 — SceneHash MUST be SHA-256
    (void)bytes;
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

PreparedScene IrLoader::CreateTestScene(uint32_t width, uint32_t height) {
    PreparedScene scene;
    scene.scene_id = "test/simple_rect";
    scene.scene_hash = "test_scene_hash";
    scene.ir_major_version = kIrMajorVersion;
    scene.ir_minor_version = kIrMinorVersion;
    scene.width = width;
    scene.height = height;

    // Add a simple solid red paint
    Paint red_paint;
    red_paint.type = PaintType::kSolid;
    red_paint.color = 0xFF0000FF;  // RGBA: Red, fully opaque
    scene.paints.push_back(red_paint);

    // Add a simple rectangle path
    Path rect;
    rect.verbs = {PathVerb::kMoveTo, PathVerb::kLineTo, PathVerb::kLineTo, PathVerb::kLineTo,
                  PathVerb::kClose};
    // Rectangle at (100, 100) with size 200x150
    rect.points = {
        100.0f, 100.0f,  // MoveTo
        300.0f, 100.0f,  // LineTo
        300.0f, 250.0f,  // LineTo
        100.0f, 250.0f,  // LineTo
    };
    scene.paths.push_back(rect);

    // Create command stream
    // Clear to white, set fill to red, fill the rectangle
    scene.command_stream = {
        // Clear(0xFFFFFFFF) - White background
        static_cast<uint8_t>(Opcode::kClear), 0xFF, 0xFF, 0xFF, 0xFF,

        // SetFill(paint_id=0, rule=NonZero)
        static_cast<uint8_t>(Opcode::kSetFill), 0x00, 0x00,  // paint_id = 0 (little-endian u16)
        0x00,                                                // rule = NonZero

        // FillPath(path_id=0)
        static_cast<uint8_t>(Opcode::kFillPath), 0x00, 0x00,  // path_id = 0 (little-endian u16)

        // End
        static_cast<uint8_t>(Opcode::kEnd)};

    return scene;
}

}  // namespace ir
}  // namespace vgcpu
