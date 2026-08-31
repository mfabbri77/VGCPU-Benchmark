// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

#include "pal/memory_tracker.h"

#include <atomic>
#include <cstdlib>

#if defined(__linux__) || defined(__GLIBC__)
#include <dlfcn.h>
#include <malloc.h>

namespace {

struct InternalStats {
    int64_t alloc_count = 0;
    int64_t free_count = 0;
    int64_t total_alloc_bytes = 0;
    int64_t current_live_bytes = 0;
    int64_t peak_heap_bytes = 0;
};

thread_local bool t_tracking = false;
thread_local InternalStats t_stats;

typedef void* (*malloc_fn)(size_t);
typedef void (*free_fn)(void*);
typedef void* (*calloc_fn)(size_t, size_t);
typedef void* (*realloc_fn)(void*, size_t);
typedef int (*posix_memalign_fn)(void**, size_t, size_t);
typedef void* (*aligned_alloc_fn)(size_t, size_t);

malloc_fn real_malloc = nullptr;
free_fn real_free = nullptr;
calloc_fn real_calloc = nullptr;
realloc_fn real_realloc = nullptr;
posix_memalign_fn real_posix_memalign = nullptr;
aligned_alloc_fn real_aligned_alloc = nullptr;

void InitHooks() {
    if (!real_malloc) {
        real_malloc = (malloc_fn)dlsym(RTLD_NEXT, "malloc");
        real_free = (free_fn)dlsym(RTLD_NEXT, "free");
        real_calloc = (calloc_fn)dlsym(RTLD_NEXT, "calloc");
        real_realloc = (realloc_fn)dlsym(RTLD_NEXT, "realloc");
        real_posix_memalign = (posix_memalign_fn)dlsym(RTLD_NEXT, "posix_memalign");
        real_aligned_alloc = (aligned_alloc_fn)dlsym(RTLD_NEXT, "aligned_alloc");
    }
}

}  // namespace

extern "C" {

void* malloc(size_t size) {
    if (!real_malloc)
        InitHooks();
    void* ptr = real_malloc ? real_malloc(size) : nullptr;
    if (t_tracking && ptr) {
        size_t actual = malloc_usable_size(ptr);
        t_stats.alloc_count++;
        t_stats.total_alloc_bytes += actual;
        t_stats.current_live_bytes += actual;
        if (t_stats.current_live_bytes > t_stats.peak_heap_bytes) {
            t_stats.peak_heap_bytes = t_stats.current_live_bytes;
        }
    }
    return ptr;
}

void free(void* ptr) {
    if (!real_free)
        InitHooks();
    if (t_tracking && ptr) {
        size_t actual = malloc_usable_size(ptr);
        t_stats.free_count++;
        t_stats.current_live_bytes -= actual;
    }
    if (real_free)
        real_free(ptr);
}

void* calloc(size_t nmemb, size_t size) {
    if (!real_calloc) {
        // dlsym may call calloc during initialization
        static char tmp_buf[16384];
        static size_t tmp_pos = 0;
        size_t total = nmemb * size;
        if (tmp_pos + total <= sizeof(tmp_buf)) {
            void* p = tmp_buf + tmp_pos;
            tmp_pos += (total + 15) & ~15;
            return p;
        }
        InitHooks();
    }
    void* ptr = real_calloc ? real_calloc(nmemb, size) : nullptr;
    if (t_tracking && ptr) {
        size_t actual = malloc_usable_size(ptr);
        t_stats.alloc_count++;
        t_stats.total_alloc_bytes += actual;
        t_stats.current_live_bytes += actual;
        if (t_stats.current_live_bytes > t_stats.peak_heap_bytes) {
            t_stats.peak_heap_bytes = t_stats.current_live_bytes;
        }
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!real_realloc)
        InitHooks();
    size_t old_size = (ptr && t_tracking) ? malloc_usable_size(ptr) : 0;
    void* new_ptr = real_realloc ? real_realloc(ptr, size) : nullptr;
    if (t_tracking && new_ptr) {
        size_t new_size = malloc_usable_size(new_ptr);
        t_stats.alloc_count++;
        t_stats.total_alloc_bytes += new_size;
        t_stats.current_live_bytes += (int64_t)new_size - (int64_t)old_size;
        if (t_stats.current_live_bytes > t_stats.peak_heap_bytes) {
            t_stats.peak_heap_bytes = t_stats.current_live_bytes;
        }
    }
    return new_ptr;
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    if (!real_posix_memalign)
        InitHooks();
    int res = real_posix_memalign ? real_posix_memalign(memptr, alignment, size) : -1;
    if (t_tracking && res == 0 && memptr && *memptr) {
        size_t actual = malloc_usable_size(*memptr);
        t_stats.alloc_count++;
        t_stats.total_alloc_bytes += actual;
        t_stats.current_live_bytes += actual;
        if (t_stats.current_live_bytes > t_stats.peak_heap_bytes) {
            t_stats.peak_heap_bytes = t_stats.current_live_bytes;
        }
    }
    return res;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if (!real_aligned_alloc)
        InitHooks();
    void* ptr = real_aligned_alloc ? real_aligned_alloc(alignment, size) : nullptr;
    if (t_tracking && ptr) {
        size_t actual = malloc_usable_size(ptr);
        t_stats.alloc_count++;
        t_stats.total_alloc_bytes += actual;
        t_stats.current_live_bytes += actual;
        if (t_stats.current_live_bytes > t_stats.peak_heap_bytes) {
            t_stats.peak_heap_bytes = t_stats.current_live_bytes;
        }
    }
    return ptr;
}

}  // extern "C"

namespace vgcpu::pal {

MemoryProfile TrackMemory(const std::function<void()>& fn) {
    InitHooks();
    t_stats = {};
    t_tracking = true;
    fn();
    t_tracking = false;

    MemoryProfile profile;
    profile.alloc_count = t_stats.alloc_count;
    profile.free_count = t_stats.free_count;
    profile.peak_heap_bytes = t_stats.peak_heap_bytes;
    profile.total_alloc_bytes = t_stats.total_alloc_bytes;
    profile.leaked_bytes = t_stats.current_live_bytes;
    return profile;
}

}  // namespace vgcpu::pal

#else

namespace vgcpu::pal {

MemoryProfile TrackMemory(const std::function<void()>& fn) {
    fn();
    return MemoryProfile{};
}

}  // namespace vgcpu::pal

#endif
