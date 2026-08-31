// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace vgcpu::pal {

/// Memory profile metrics collected for a single rendering frame.
struct MemoryProfile {
    int64_t alloc_count =
        0;  ///< Total dynamic allocation calls (malloc/calloc/realloc/posix_memalign/new)
    int64_t free_count = 0;       ///< Total deallocations (free/delete)
    int64_t peak_heap_bytes = 0;  ///< High-water mark of live heap memory during the tracked scope
    int64_t total_alloc_bytes = 0;  ///< Cumulative bytes requested across all allocations
    int64_t leaked_bytes = 0;       ///< Net live heap memory remaining at end of scope
};

/// Track memory allocations during the execution of a callable.
/// Thread-safe: uses thread-local tracking flag and counters.
/// When not tracking, overhead is zero (predicted-false branch).
MemoryProfile TrackMemory(const std::function<void()>& fn);

}  // namespace vgcpu::pal
