// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-09] Statistics (Chapter 3) / [API-06-06] SceneStats (Chapter 4)

#include "harness/statistics.h"

#include <algorithm>
#include <cmath>

namespace vgcpu {

int64_t ComputePercentile(const std::vector<int64_t>& sorted, double percentile) {
    if (sorted.empty()) {
        return 0;
    }

    double index = (percentile / 100.0) * static_cast<double>(sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));

    if (lower == upper) {
        return sorted[lower];
    }

    // Linear interpolation
    double fraction = index - static_cast<double>(lower);
    return static_cast<int64_t>(static_cast<double>(sorted[lower]) * (1.0 - fraction) +
                                static_cast<double>(sorted[upper]) * fraction);
}

TimingStats ComputeStats(std::vector<int64_t>& wall_samples, std::vector<int64_t>& cpu_samples) {
    TimingStats stats;

    if (wall_samples.empty() || cpu_samples.empty()) {
        return stats;
    }

    stats.sample_count = static_cast<int>(wall_samples.size());

    // Sort samples for percentile calculation
    std::sort(wall_samples.begin(), wall_samples.end());
    std::sort(cpu_samples.begin(), cpu_samples.end());

    // Compute percentiles
    // Blueprint Reference: [ARCH-12-02c] SceneStats (Chapter 3) / [DEC-MEM-03] Percentiles from
    // sorted samples (Chapter 5)
    stats.wall_p50_ns = ComputePercentile(wall_samples, 50.0);
    stats.wall_p90_ns = ComputePercentile(wall_samples, 90.0);
    stats.cpu_p50_ns = ComputePercentile(cpu_samples, 50.0);
    stats.cpu_p90_ns = ComputePercentile(cpu_samples, 90.0);

    return stats;
}

}  // namespace vgcpu
