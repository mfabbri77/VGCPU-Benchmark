// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#include "vgcpu/internal/alloc_tracker.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace vgcpu {
namespace internal {

static std::atomic<bool> g_enabled{false};
static std::atomic<size_t> g_alloc_count{0};
static std::atomic<size_t> g_dealloc_count{0};
static std::atomic<size_t> g_total_bytes{0};

void AllocTracker::Reset() {
    g_alloc_count = 0;
    g_dealloc_count = 0;
    g_total_bytes = 0;
}

void AllocTracker::Enable() {
    g_enabled = true;
}
void AllocTracker::Disable() {
    g_enabled = false;
}
bool AllocTracker::IsEnabled() {
    return g_enabled;
}

size_t AllocTracker::GetAllocationCount() {
    return g_alloc_count;
}
size_t AllocTracker::GetDeallocationCount() {
    return g_dealloc_count;
}
size_t AllocTracker::GetTotalAllocatedBytes() {
    return g_total_bytes;
}

void AllocTracker::RecordAllocation(size_t size) {
    if (g_enabled) {
        g_alloc_count++;
        g_total_bytes += size;
    }
}

void AllocTracker::RecordDeallocation() {
    if (g_enabled) {
        g_dealloc_count++;
    }
}

}  // namespace internal
}  // namespace vgcpu

#ifdef VGCPU_ENABLE_ALLOC_INSTRUMENTATION

void* operator new(std::size_t size) {
    vgcpu::internal::AllocTracker::RecordAllocation(size);
    void* p = std::malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    vgcpu::internal::AllocTracker::RecordDeallocation();
    std::free(p);
}

void* operator new[](std::size_t size) {
    vgcpu::internal::AllocTracker::RecordAllocation(size);
    void* p = std::malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    vgcpu::internal::AllocTracker::RecordDeallocation();
    std::free(p);
}

// C++17 overloads for alignment if needed, but keeping it simple for now as per blueprint.
#endif
