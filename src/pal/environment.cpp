// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-03] PAL (Chapter 3) / [API-06-02] PAL (Chapter 4)
// Blueprint Reference: [ARCH-12-02d] RunReport environment metadata (Chapter 3)

#include "pal/environment.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <fstream>
#endif

namespace vgcpu {
namespace pal {

EnvironmentInfo CollectEnvironment() {
    EnvironmentInfo info;

#if defined(_WIN32)
    // OS Name and Version
    info.os_name = "Windows";
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    // Note: GetVersionEx is deprecated, but works for basic info
    info.os_version = "Unknown";

    // Architecture
#if defined(_M_X64) || defined(__x86_64__)
    info.arch = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    info.arch = "arm64";
#else
    info.arch = "unknown";
#endif

    // CPU Model
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info.cpu_cores = static_cast<int>(sysinfo.dwNumberOfProcessors);

    // Memory
    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    if (GlobalMemoryStatusEx(&memstat)) {
        info.memory_bytes = static_cast<int64_t>(memstat.ullTotalPhys);
    }

#elif defined(__APPLE__)
    // OS Name and Version
    info.os_name = "macOS";
    struct utsname uts;
    if (uname(&uts) == 0) {
        info.os_version = uts.release;
    }

    // Architecture
#if defined(__x86_64__)
    info.arch = "x86_64";
#elif defined(__aarch64__)
    info.arch = "arm64";
#else
    info.arch = "unknown";
#endif

    // CPU Model
    char cpu_brand[256] = {};
    size_t size = sizeof(cpu_brand);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &size, nullptr, 0) == 0) {
        info.cpu_model = cpu_brand;
    }

    // CPU Cores
    int cores = 0;
    size = sizeof(cores);
    if (sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0) == 0) {
        info.cpu_cores = cores;
    }

    // Memory
    int64_t mem = 0;
    size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == 0) {
        info.memory_bytes = mem;
    }

#elif defined(__linux__)
    // OS Name and Version
    info.os_name = "Linux";
    struct utsname uts;
    if (uname(&uts) == 0) {
        info.os_version = uts.release;
    }

    // Architecture
#if defined(__x86_64__)
    info.arch = "x86_64";
#elif defined(__aarch64__)
    info.arch = "arm64";
#else
    info.arch = "unknown";
#endif

    // CPU Model (from /proc/cpuinfo)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                info.cpu_model = line.substr(pos + 2);
            }
            break;
        }
    }

    // CPU Cores
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.memory_bytes = static_cast<int64_t>(si.totalram) * si.mem_unit;
    }

    // Count CPUs from sysconf
    info.cpu_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

#endif

    // Compiler info (compile-time)
#if defined(__clang__)
    info.compiler_name = "Clang";
    info.compiler_version = __clang_version__;
#elif defined(__GNUC__)
    info.compiler_name = "GCC";
    info.compiler_version = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." +
                            std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    info.compiler_name = "MSVC";
    info.compiler_version = std::to_string(_MSC_VER);
#else
    info.compiler_name = "Unknown";
    info.compiler_version = "Unknown";
#endif

    return info;
}

std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = {};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}

}  // namespace pal
}  // namespace vgcpu
