#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vgcpu::artifacts {

/**
 * @brief Writes an RGBA8 buffer to a PNG file.
 *
 * @param path Output file path.
 * @param width Image width.
 * @param height Image height.
 * @param rgba_data Buffer containing RGBA8 pixels.
 * @param stride Bytes per row (stride).
 * @return true if successful, false otherwise.
 */
bool write_png(const std::string& path, int width, int height, std::span<const uint8_t> rgba_data,
               int stride = 0);

}  // namespace vgcpu::artifacts
