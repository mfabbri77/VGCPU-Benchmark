// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.3 — Module pal (Platform Abstraction Layer)
// Blueprint Reference: Chapter 6, §6.5.1 — Timer Abstraction

#include "pal/timer.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#endif

namespace vgcpu {
namespace pal {

TimePoint NowMonotonic() {
    return std::chrono::steady_clock::now();
}

Duration Elapsed(TimePoint start, TimePoint end) {
    return std::chrono::duration_cast<Duration>(end - start);
}

#if defined(_WIN32)

// Windows: Use GetProcessTimes for process CPU time
Duration GetCpuTime() {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time,
                        &user_time)) {
        // FILETIME is in 100-nanosecond intervals
        ULARGE_INTEGER kernel, user;
        kernel.LowPart = kernel_time.dwLowDateTime;
        kernel.HighPart = kernel_time.dwHighDateTime;
        user.LowPart = user_time.dwLowDateTime;
        user.HighPart = user_time.dwHighDateTime;

        // Convert to nanoseconds (multiply by 100)
        int64_t total_ns = static_cast<int64_t>((kernel.QuadPart + user.QuadPart) * 100);
        return Duration(total_ns);
    }
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "process";
}

#elif defined(__APPLE__)

// macOS: Use clock_gettime with CLOCK_PROCESS_CPUTIME_ID if available,
// otherwise fall back to rusage
#if defined(CLOCK_PROCESS_CPUTIME_ID)
Duration GetCpuTime() {
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
        return Duration(static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec);
    }
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "process";
}
#else
#include <sys/resource.h>

Duration GetCpuTime() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        int64_t user_ns = static_cast<int64_t>(usage.ru_utime.tv_sec) * 1'000'000'000 +
                          static_cast<int64_t>(usage.ru_utime.tv_usec) * 1000;
        int64_t sys_ns = static_cast<int64_t>(usage.ru_stime.tv_sec) * 1'000'000'000 +
                         static_cast<int64_t>(usage.ru_stime.tv_usec) * 1000;
        return Duration(user_ns + sys_ns);
    }
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "process";
}
#endif

#elif defined(__linux__)

// Linux: Use clock_gettime with CLOCK_PROCESS_CPUTIME_ID
Duration GetCpuTime() {
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
        return Duration(static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec);
    }
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "process";
}

#else

// Fallback: Return 0 for unsupported platforms
Duration GetCpuTime() {
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "unsupported";
}

#endif

}  // namespace pal
}  // namespace vgcpu
