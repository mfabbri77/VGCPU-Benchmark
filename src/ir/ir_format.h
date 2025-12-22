// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 5, §5.3.2 — IR Binary Layout

#pragma once

#include <array>
#include <cstdint>

namespace vgcpu {
namespace ir {

/// IR file magic bytes: 'V', 'G', 'I', 'R'
/// Blueprint Reference: Chapter 5, §5.3.2 — File Header
constexpr std::array<uint8_t, 4> kIrMagic = {'V', 'G', 'I', 'R'};

/// Current IR format version.
constexpr uint8_t kIrMajorVersion = 1;
constexpr uint8_t kIrMinorVersion = 0;

/// IR File Header (16 bytes, little-endian).
/// Blueprint Reference: Chapter 5, §5.3.2 — File Header
struct IrHeader {
    uint8_t magic[4];     ///< 'V', 'G', 'I', 'R'
    uint8_t major_ver;    ///< Major version (1)
    uint8_t minor_ver;    ///< Minor version (0)
    uint16_t reserved;    ///< Reserved (0x0000)
    uint32_t total_size;  ///< Total file size in bytes
    uint32_t scene_crc;   ///< CRC32 of scene content (excluding header)
};

static_assert(sizeof(IrHeader) == 16, "IrHeader must be exactly 16 bytes");

/// Section Type IDs.
/// Blueprint Reference: Chapter 5, §5.3.2 — Section Type IDs
enum class SectionType : uint8_t {
    kInfo = 0x01,       ///< Metadata using key-value pairs
    kPaint = 0x02,      ///< Color/Gradient table
    kPath = 0x03,       ///< Path geometry table
    kCommand = 0x04,    ///< The rendering command stream
    kExtension = 0xFF,  ///< Extension section
};

/// Section Header.
/// Blueprint Reference: Chapter 5, §5.3.2 — Sections
/// Note: Binary format uses 6-byte layout (type:u8, reserved:u8, length:u32).
/// In-memory representation may differ due to alignment.
struct SectionHeader {
    SectionType type;  ///< SectionTypeID
    uint8_t reserved;  ///< Reserved (0)
    uint32_t length;   ///< Section length in bytes (including this header)
};

// Binary layout constants for parsing
constexpr size_t kSectionHeaderBinarySize = 6;

/// Command Opcodes.
/// Blueprint Reference: Chapter 5, §5.3.3 — IR Opcode Reference
enum class Opcode : uint8_t {
    kEnd = 0x00,           ///< End of stream
    kSave = 0x01,          ///< Push state (matrix, clip, paints)
    kRestore = 0x02,       ///< Pop state
    kClear = 0x10,         ///< Clear canvas (rgba:u32)
    kSetMatrix = 0x20,     ///< Set current transform (m:f32[6])
    kConcatMatrix = 0x21,  ///< Multiply current transform (m:f32[6])
    kSetFill = 0x30,       ///< Set fill paint & rule (paint_id:u16, rule:u8)
    kSetStroke = 0x31,     ///< Set stroke paint & params (paint_id:u16, width:f32, opts:u8)
    kFillPath = 0x40,      ///< Fill path at index (path_id:u16)
    kStrokePath = 0x41,    ///< Stroke path at index (path_id:u16)
};

/// Fill rule encoding (u8 in SetFill).
/// Blueprint Reference: Chapter 5, §5.3.3 — SetFill opcode
enum class FillRule : uint8_t {
    kNonZero = 0,
    kEvenOdd = 1,
};

/// Stroke Options bitfield (u8 in SetStroke).
/// Blueprint Reference: Chapter 5, §5.3.6 — Stroke Options
/// Bits 0-1: Cap (0=Butt, 1=Round, 2=Square)
/// Bits 2-3: Join (0=Miter, 1=Round, 2=Bevel)
/// Bits 4-7: Reserved
enum class StrokeCap : uint8_t {
    kButt = 0,
    kRound = 1,
    kSquare = 2,
};

enum class StrokeJoin : uint8_t {
    kMiter = 0,
    kRound = 1,
    kBevel = 2,
};

/// Utility to pack stroke options into a single byte.
inline uint8_t PackStrokeOptions(StrokeCap cap, StrokeJoin join) {
    return static_cast<uint8_t>(cap) | (static_cast<uint8_t>(join) << 2);
}

/// Utility to unpack stroke cap from options byte.
inline StrokeCap UnpackStrokeCap(uint8_t opts) {
    return static_cast<StrokeCap>(opts & 0x03);
}

/// Utility to unpack stroke join from options byte.
inline StrokeJoin UnpackStrokeJoin(uint8_t opts) {
    return static_cast<StrokeJoin>((opts >> 2) & 0x03);
}

/// Path verb codes.
/// Blueprint Reference: Chapter 5, §5.3.4 — Verb Codes
enum class PathVerb : uint8_t {
    kMoveTo = 0,   ///< Move to (1 point)
    kLineTo = 1,   ///< Line to (1 point)
    kQuadTo = 2,   ///< Quadratic bezier (2 points: control, end)
    kCubicTo = 3,  ///< Cubic bezier (3 points: c1, c2, end)
    kClose = 4,    ///< Close path (0 points)
};

/// Paint type.
/// Blueprint Reference: Chapter 5, §5.3.5 — Paint Entry Format
enum class PaintType : uint8_t {
    kSolid = 0,
    kLinear = 1,
    kRadial = 2,
};

/// Gradient stop.
struct GradientStop {
    float offset;    ///< Position [0, 1]
    uint32_t color;  ///< RGBA8 premultiplied
};

}  // namespace ir
}  // namespace vgcpu
