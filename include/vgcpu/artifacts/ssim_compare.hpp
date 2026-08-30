#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace vgcpu::artifacts {

struct SsimResult {
    double score;  // 0.0 to 1.0 (1.0 = identical)
    bool passed;   // score >= threshold
    std::string message;
    // Worst-case complement to SSIM (industry pair, cf. ImageMagick
    // PAE/AE): SSIM averages structure, so a single badly-wrong pixel can
    // hide inside a ~1.0 score. PAE is the L-infinity norm.
    int pae = 0;            // peak absolute error: max per-channel |a-b|, 0..255
    double ae_ratio = 0.0;  // fraction of pixels with any channel diff > kAeTolerance
    static constexpr int kAeTolerance = 8;  // "fuzz": AA/rounding allowance
};

/**
 * @brief Computes SSIM between two RGBA8 buffers.
 *
 * @param width Image width.
 * @param height Image height.
 * @param buf_a First buffer (reference).
 * @param stride_a Stride of first buffer.
 * @param buf_b Second buffer (test).
 * @param stride_b Stride of second buffer.
 * @return SsimResult
 */
SsimResult compute_ssim(int width, int height, std::span<const uint8_t> buf_a, int stride_a,
                        std::span<const uint8_t> buf_b, int stride_b);

}  // namespace vgcpu::artifacts
