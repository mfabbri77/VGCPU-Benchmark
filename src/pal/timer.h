// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-03] PAL (Chapter 3) / [API-06-02] PAL (Chapter 4)
// Blueprint Reference: [REQ-26] Monotonic timing (Chapter 4)

#pragma once

#include <chrono>
#include <cstdint>

namespace vgcpu {
namespace pal {

/// High-resolution time point type.
using TimePoint = std::chrono::steady_clock::time_point;

/// Duration type in nanoseconds.
using Duration = std::chrono::nanoseconds;

/// Get the current monotonic time.
/// Blueprint Reference: [API-06-02] NowMonotonicNs (Chapter 4) / [REQ-26] Monotonic timing (Chapter
/// 4)
[[nodiscard]] TimePoint NowMonotonic();

/// Calculate elapsed time between two time points.
/// Blueprint Reference: [ARCH-10-03] PAL Utilities (Chapter 3) / [API-06-02] NowMonotonicNs
/// (Chapter 4)
[[nodiscard]] Duration Elapsed(TimePoint start, TimePoint end);

/// Get the current process/thread CPU time.
/// Blueprint Reference: [ARCH-10-03] PAL Environment (Chapter 3) / [API-06-02] EnvInfo (Chapter 4)
/// Note: Semantics (process vs thread) vary by platform and are reported in metadata.
[[nodiscard]] Duration GetCpuTime();

/// Get the CPU time measurement semantics for the current platform.
/// Returns "process" or "thread" depending on what GetCpuTime() measures.
[[nodiscard]] const char* GetCpuTimeSemantics();

/// Get the estimated CPU frequency in Hz (0 if unknown).
[[nodiscard]] int64_t GetCpuFrequency();

/// Convert duration to nanoseconds as int64.
[[nodiscard]] inline int64_t ToNanoseconds(Duration d) {
    return d.count();
}

/// Convert duration to microseconds as double.
[[nodiscard]] inline double ToMicroseconds(Duration d) {
    return static_cast<double>(d.count()) / 1000.0;
}

/// Convert duration to milliseconds as double.
[[nodiscard]] inline double ToMilliseconds(Duration d) {
    return static_cast<double>(d.count()) / 1'000'000.0;
}

/// Convert duration to seconds as double.
[[nodiscard]] inline double ToSeconds(Duration d) {
    return static_cast<double>(d.count()) / 1'000'000'000.0;
}

}  // namespace pal
}  // namespace vgcpu
