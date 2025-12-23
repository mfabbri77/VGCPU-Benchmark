#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vgcpu::artifacts {

/**
 * @brief Reads an image file (PNG/JPG/etc) into an RGBA8 buffer.
 *
 * @param path Path to the image file.
 * @param width Output width.
 * @param height Output height.
 * @return Vector containing RGBA8 pixels, or empty on failure.
 */
std::vector<uint8_t> read_image(const std::string& path, int& width, int& height);

}  // namespace vgcpu::artifacts
