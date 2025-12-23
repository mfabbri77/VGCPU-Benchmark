// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.3 — Allocation Instrumentation [REQ-113]

#pragma once

#include <cstddef>
#include <cstdint>

namespace vgcpu {
namespace internal {

/// Tracks memory allocations for hot-path enforcement.
class AllocTracker {
   public:
    static void Reset();
    static void Enable();
    static void Disable();
    static bool IsEnabled();
    static size_t GetAllocationCount();
    static size_t GetDeallocationCount();
    static size_t GetTotalAllocatedBytes();

    /// Internal use only by overloaded operator new/delete
    static void RecordAllocation(size_t size);
    static void RecordDeallocation();
};

/// Scoped guard to monitor allocations within a code block.
class ScopedAllocationGuard {
   public:
    ScopedAllocationGuard() {
        AllocTracker::Reset();
        AllocTracker::Enable();
    }
    ~ScopedAllocationGuard() { AllocTracker::Disable(); }

    [[nodiscard]] size_t GetAllocationCount() const { return AllocTracker::GetAllocationCount(); }
};

}  // namespace internal
}  // namespace vgcpu
