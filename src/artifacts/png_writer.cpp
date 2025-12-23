#include "vgcpu/artifacts/png_writer.hpp"

#include <iostream>

// Forward declaration from stb_image_write.h
extern "C" int stbi_write_png(char const* filename, int w, int h, int comp, const void* data,
                              int stride_in_bytes);

namespace vgcpu::artifacts {

bool write_png(const std::string& path, int width, int height, std::span<const uint8_t> rgba_data,
               int stride) {
    if (width <= 0 || height <= 0 || rgba_data.empty()) {
        std::cerr << "Invalid dimensions or data for PNG write: " << path << "\n";
        return false;
    }

    // Default stride: tightly packed RGBA
    if (stride == 0) {
        stride = width * 4;
    }

    // stbi_write_png returns non-zero on success, 0 on failure
    int m = stbi_write_png(path.c_str(), width, height, 4, rgba_data.data(), stride);
    if (m == 0) {
        std::cerr << "stbi_write_png failed for: " << path << "\n";
        return false;
    }
    return true;
}

}  // namespace vgcpu::artifacts
