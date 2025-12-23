// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-09] Statistics (Chapter 3) / [API-06-06] SceneStats (Chapter 4)

#pragma once

#include "harness/harness.h"

#include <cstdint>
#include <vector>

namespace vgcpu {

/// Compute timing statistics from samples.
/// Blueprint Reference: [ARCH-12-02c] SceneStats (Chapter 3) / [ARCH-10-09] (Chapter 3)
/// @param wall_samples Wall time samples in nanoseconds.
/// @param cpu_samples CPU time samples in nanoseconds.
/// @return Computed statistics.
TimingStats ComputeStats(std::vector<int64_t>& wall_samples, std::vector<int64_t>& cpu_samples);

/// Compute the median (p50) of a sorted array.
int64_t ComputePercentile(const std::vector<int64_t>& sorted, double percentile);

}  // namespace vgcpu
