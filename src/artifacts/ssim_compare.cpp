#include "vgcpu/artifacts/ssim_compare.hpp"

#include "ssim_lomont/ssim_lomont.hpp"

#include <iostream>
#include <string>

namespace vgcpu::artifacts {

SsimResult compute_ssim(int width, int height, std::span<const uint8_t> buf_a, int stride_a,
                        std::span<const uint8_t> buf_b, int stride_b) {
    if (width <= 0 || height <= 0 || buf_a.empty() || buf_b.empty()) {
        return {0.0, false, "Invalid input dimensions or buffers"};
    }

    using namespace Lomont::Graphics;

    // Helper to get normalized grayscale [0,1] from RGBA buffer
    auto get_pixel_a = [&](int i, int j) -> double {
        // Clamp bounds (though SSIM internal loops should be safe)
        if (i < 0 || i >= width || j < 0 || j >= height)
            return 0.0;

        // Calculate offset: y * stride + x * bpp
        const size_t offset = static_cast<size_t>(j) * stride_a + static_cast<size_t>(i) * 4;
        if (offset + 2 >= buf_a.size())
            return 0.0;  // Safety

        const uint8_t* p = buf_a.data() + offset;
        return ImageMetrics::Rgb2Gray(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0);
    };

    auto get_pixel_b = [&](int i, int j) -> double {
        if (i < 0 || i >= width || j < 0 || j >= height)
            return 0.0;

        const size_t offset = static_cast<size_t>(j) * stride_b + static_cast<size_t>(i) * 4;
        if (offset + 2 >= buf_b.size())
            return 0.0;

        const uint8_t* p = buf_b.data() + offset;
        return ImageMetrics::Rgb2Gray(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0);
    };

    // Compute SSIM
    double score = ImageMetrics::SSIM(width, height, get_pixel_a, get_pixel_b);

    // Threshold for regression testing (configurable in future?)
    const double threshold = 0.99;
    bool passed = (score >= threshold);

    std::string msg = passed ? "SSIM passed" : "SSIM failed";
    if (!passed) {
        msg += " (score=" + std::to_string(score) + " < " + std::to_string(threshold) + ")";
    }

    return {score, passed, msg};
}

}  // namespace vgcpu::artifacts
