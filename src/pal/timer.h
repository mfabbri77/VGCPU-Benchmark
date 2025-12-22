// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.3 — Module pal (Platform Abstraction Layer)
// Blueprint Reference: Chapter 6, §6.5.1 — Timer Abstraction

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
/// Blueprint Reference: Chapter 6, §6.5.1 — NowMonotonic()
[[nodiscard]] TimePoint NowMonotonic();

/// Calculate elapsed time between two time points.
/// Blueprint Reference: Chapter 6, §6.5.1 — Elapsed()
[[nodiscard]] Duration Elapsed(TimePoint start, TimePoint end);

/// Get the current process/thread CPU time.
/// Blueprint Reference: Chapter 6, §6.5.1 — GetCpuTime()
/// Note: Semantics (process vs thread) vary by platform and are reported in metadata.
[[nodiscard]] Duration GetCpuTime();

/// Get the CPU time measurement semantics for the current platform.
/// Returns "process" or "thread" depending on what GetCpuTime() measures.
[[nodiscard]] const char* GetCpuTimeSemantics();

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
