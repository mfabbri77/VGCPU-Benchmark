// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: Chapter 7, §7.2.3 — Module pal (Platform Abstraction Layer)
// Blueprint Reference: Chapter 6, §6.5.1 — Timer Abstraction

#include "pal/timer.h"

#include <atomic>
#include <mutex>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Link with Kernel32.lib
#elif defined(__APPLE__) || defined(__linux__)
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#endif

namespace vgcpu {
namespace pal {

namespace {
std::atomic<int64_t> g_cpu_frequency{0};
}

TimePoint NowMonotonic() {
    return std::chrono::steady_clock::now();
}

Duration Elapsed(TimePoint start, TimePoint end) {
    return std::chrono::duration_cast<Duration>(end - start);
}

// Forward declaration of calibration helper (platform specific)
static void EnsureTimerCalibrated();

int64_t GetCpuFrequency() {
    EnsureTimerCalibrated();
    return g_cpu_frequency.load(std::memory_order_relaxed);
}

#if defined(_WIN32)

// Windows: Use QueryProcessCycleTime and runtime calibration
namespace {
void CalibrateWindowTimer() {
    if (g_cpu_frequency.load(std::memory_order_relaxed) != 0)
        return;

    static std::once_flag flag;
    std::call_once(flag, []() {
        HANDLE process = GetCurrentProcess();
        ULONG64 start_cycles = 0;
        ULONG64 end_cycles = 0;

        QueryProcessCycleTime(process, &start_cycles);
        auto start_wall = std::chrono::steady_clock::now();
        QueryProcessCycleTime(process, &start_cycles);

        while (true) {
            if ((std::chrono::steady_clock::now() - start_wall) > std::chrono::milliseconds(100))
                break;
        }

        auto end_wall = std::chrono::steady_clock::now();
        QueryProcessCycleTime(process, &end_cycles);

        auto wall_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end_wall - start_wall).count();
        auto cycles = end_cycles - start_cycles;

        if (wall_ns > 0 && cycles > 0) {
            double freq = static_cast<double>(cycles) * 1e9 / static_cast<double>(wall_ns);
            g_cpu_frequency.store(static_cast<int64_t>(freq), std::memory_order_relaxed);
        } else {
            g_cpu_frequency.store(3'000'000'000, std::memory_order_relaxed);
        }
    });
}
}  // namespace

static void EnsureTimerCalibrated() {
    CalibrateWindowTimer();
}

Duration GetCpuTime() {
    EnsureTimerCalibrated();
    ULONG64 cycles = 0;
    if (QueryProcessCycleTime(GetCurrentProcess(), &cycles)) {
        int64_t freq = g_cpu_frequency.load(std::memory_order_relaxed);
        if (freq > 0) {
            double ns = (static_cast<double>(cycles) * 1e9) / static_cast<double>(freq);
            return Duration(static_cast<int64_t>(ns));
        }
    }
    return Duration(0);
}

const char* GetCpuTimeSemantics() {
    return "process (cycles)";
}

#elif defined(__APPLE__)

static void EnsureTimerCalibrated() {}

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
        return Duration(static_cast<int64_t>(usage.ru_utime.tv_sec) * 1'000'000'000 +
                        static_cast<int64_t>(usage.ru_utime.tv_usec) * 1000 +
                        static_cast<int64_t>(usage.ru_stime.tv_sec) * 1'000'000'000 +
                        static_cast<int64_t>(usage.ru_stime.tv_usec) * 1000);
    }
    return Duration(0);
}
const char* GetCpuTimeSemantics() {
    return "process";
}
#endif

#elif defined(__linux__)

static void EnsureTimerCalibrated() {}

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

static void EnsureTimerCalibrated() {}

Duration GetCpuTime() {
    return Duration(0);
}
const char* GetCpuTimeSemantics() {
    return "unsupported";
}

#endif

}  // namespace pal
}  // namespace vgcpu
