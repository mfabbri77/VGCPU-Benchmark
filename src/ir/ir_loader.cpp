// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 6, §6.3.1 — IR Runtime API

#include "ir/ir_loader.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

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

    if (bytes.size() < sizeof(IrHeader)) {
        report.valid = false;
        report.errors.push_back("File too small: missing IR header");
        return report;
    }

    if (bytes[0] != 'V' || bytes[1] != 'G' || bytes[2] != 'I' || bytes[3] != 'R') {
        report.valid = false;
        report.errors.push_back("Invalid magic bytes: expected 'VGIR'");
        return report;
    }

    uint8_t major = bytes[4];
    if (major != kIrMajorVersion) {
        report.valid = false;
        report.errors.push_back("Unsupported IR major version: " + std::to_string(major));
        return report;
    }

    IrHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.total_size != bytes.size()) {
        report.valid = false;
        report.errors.push_back("Size mismatch: header says " + std::to_string(header.total_size) +
                                " but file is " + std::to_string(bytes.size()) + " bytes");
        return report;
    }

    return report;
}

namespace {

// Helper to read little-endian values
template <typename T>
T ReadLE(const uint8_t* data) {
    T value;
    std::memcpy(&value, data, sizeof(T));
    return value;
}

// Parse Paint section
bool ParsePaintSection(const uint8_t* data, size_t len, std::vector<Paint>& paints) {
    if (len < 2)
        return false;

    uint16_t count = ReadLE<uint16_t>(data);
    data += 2;
    len -= 2;

    for (uint16_t i = 0; i < count; ++i) {
        if (len < 5)
            return false;  // type(1) + color(4)

        Paint paint;
        paint.type = static_cast<PaintType>(data[0]);
        paint.color = ReadLE<uint32_t>(data + 1);
        paints.push_back(paint);

        data += 5;
        len -= 5;
    }

    return true;
}

// Parse Path section
bool ParsePathSection(const uint8_t* data, size_t len, std::vector<vgcpu::Path>& paths) {
    if (len < 2)
        return false;

    uint16_t count = ReadLE<uint16_t>(data);
    data += 2;
    len -= 2;

    for (uint16_t i = 0; i < count; ++i) {
        if (len < 4)
            return false;  // verb_count(2) + point_count(2)

        uint16_t verb_count = ReadLE<uint16_t>(data);
        uint16_t point_count = ReadLE<uint16_t>(data + 2);
        data += 4;
        len -= 4;

        vgcpu::Path path;

        // Read verbs
        for (uint16_t v = 0; v < verb_count; ++v) {
            if (len < 1)
                return false;
            path.verbs.push_back(static_cast<PathVerb>(data[0]));
            data += 1;
            len -= 1;
        }

        // Read points
        for (uint16_t p = 0; p < point_count; ++p) {
            if (len < 4)
                return false;
            float pt = ReadLE<float>(data);
            path.points.push_back(pt);
            data += 4;
            len -= 4;
        }

        paths.push_back(std::move(path));
    }

    return true;
}

}  // namespace

Result<PreparedScene> IrLoader::Prepare(const std::vector<uint8_t>& bytes,
                                        const std::string& scene_id) {
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

    // Default dimensions (may be overridden by Info section)
    scene.width = 800;
    scene.height = 600;

    // Parse sections
    size_t offset = sizeof(IrHeader);
    while (offset + kSectionHeaderBinarySize <= bytes.size()) {
        // Read section header (binary format: type:u8, reserved:u8, length:u32)
        uint8_t section_type = bytes[offset];
        uint32_t section_length = ReadLE<uint32_t>(bytes.data() + offset + 2);

        if (offset + section_length > bytes.size()) {
            return Status::Fail("Section exceeds file bounds");
        }

        const uint8_t* payload = bytes.data() + offset + kSectionHeaderBinarySize;
        size_t payload_len = section_length - kSectionHeaderBinarySize;

        switch (static_cast<SectionType>(section_type)) {
            case SectionType::kPaint:
                if (!ParsePaintSection(payload, payload_len, scene.paints)) {
                    return Status::Fail("Failed to parse Paint section");
                }
                break;

            case SectionType::kPath:
                if (!ParsePathSection(payload, payload_len, scene.paths)) {
                    return Status::Fail("Failed to parse Path section");
                }
                break;

            case SectionType::kCommand:
                // Command section is stored as raw bytes for adapter interpretation
                scene.command_stream.assign(payload, payload + payload_len);
                break;

            case SectionType::kInfo:
                // TODO: Parse Info section for scene metadata
                break;

            default:
                // Skip unknown sections
                break;
        }

        offset += section_length;
    }

    if (scene.command_stream.empty()) {
        return Status::Fail("No Command section found");
    }

    return scene;
}

std::string IrLoader::ComputeHash(const std::vector<uint8_t>& bytes) {
    // Simple hash based on CRC32 (should be SHA-256 for production)
    // Blueprint Reference: Chapter 5, §5.4.1 — SceneHash MUST be SHA-256
    uint32_t crc = 0;
    for (uint8_t b : bytes) {
        crc = (crc >> 8) ^ ((crc ^ b) * 0x1EDC6F41);
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << crc;
    return oss.str();
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
    vgcpu::Path rect;
    rect.verbs = {PathVerb::kMoveTo, PathVerb::kLineTo, PathVerb::kLineTo, PathVerb::kLineTo,
                  PathVerb::kClose};
    rect.points = {
        100.0f, 100.0f,  // MoveTo
        300.0f, 100.0f,  // LineTo
        300.0f, 250.0f,  // LineTo
        100.0f, 250.0f,  // LineTo
    };
    scene.paths.push_back(rect);

    // Create command stream
    scene.command_stream = {static_cast<uint8_t>(Opcode::kClear),
                            0xFF,
                            0xFF,
                            0xFF,
                            0xFF,

                            static_cast<uint8_t>(Opcode::kSetFill),
                            0x00,
                            0x00,  // paint_id = 0
                            0x00,  // rule = NonZero

                            static_cast<uint8_t>(Opcode::kFillPath),
                            0x00,
                            0x00,  // path_id = 0

                            static_cast<uint8_t>(Opcode::kEnd)};

    return scene;
}

}  // namespace ir
}  // namespace vgcpu
