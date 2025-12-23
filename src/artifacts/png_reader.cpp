#include "vgcpu/artifacts/png_reader.hpp"

#include "stb/stb_image.h"

#include <iostream>

namespace vgcpu::artifacts {

std::vector<uint8_t> read_image(const std::string& path, int& width, int& height) {
    int channels = 0;
    // Force 4 channels (RGBA)
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        return {};
    }

    std::vector<uint8_t> result(data, data + (width * height * 4));
    stbi_image_free(data);
    return result;
}

}  // namespace vgcpu::artifacts
