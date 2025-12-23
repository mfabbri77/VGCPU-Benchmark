#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace vgcpu::artifacts {

struct SsimResult {
    double score;  // 0.0 to 1.0 (1.0 = identical)
    bool passed;   // score >= threshold
    std::string message;
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
