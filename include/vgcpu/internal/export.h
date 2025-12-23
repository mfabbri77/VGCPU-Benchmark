// include/vgcpu/internal/export.h
// Blueprint Reference: [TASK-05.01], [REQ-42], [REQ-43]
//
// Export and visibility macros for VGCPU public API.

#pragma once

// Platform detection
#if defined(_WIN32) || defined(__CYGWIN__)
#define VGCPU_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define VGCPU_PLATFORM_MACOS 1
#else
#define VGCPU_PLATFORM_LINUX 1
#endif

// Compiler detection
#if defined(_MSC_VER)
#define VGCPU_COMPILER_MSVC 1
#elif defined(__clang__)
#define VGCPU_COMPILER_CLANG 1
#elif defined(__GNUC__)
#define VGCPU_COMPILER_GCC 1
#endif

// Export/Import macros for shared library builds
// For static library builds, VGCPU_EXPORT is empty
#if defined(VGCPU_SHARED_LIBRARY)
#if defined(VGCPU_PLATFORM_WINDOWS)
#if defined(VGCPU_BUILDING_LIBRARY)
#define VGCPU_EXPORT __declspec(dllexport)
#else
#define VGCPU_EXPORT __declspec(dllimport)
#endif
#else
#define VGCPU_EXPORT __attribute__((visibility("default")))
#endif
#else
#define VGCPU_EXPORT
#endif

// Internal visibility (not exported)
#if defined(VGCPU_COMPILER_MSVC)
#define VGCPU_INTERNAL
#else
#define VGCPU_INTERNAL __attribute__((visibility("hidden")))
#endif

// Deprecation macro
#if defined(VGCPU_COMPILER_MSVC)
#define VGCPU_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define VGCPU_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif

// No-discard for functions returning important values
#define VGCPU_NODISCARD [[nodiscard]]

// Force inline
#if defined(VGCPU_COMPILER_MSVC)
#define VGCPU_FORCEINLINE __forceinline
#else
#define VGCPU_FORCEINLINE inline __attribute__((always_inline))
#endif

// No inline (for profiling, debugging)
#if defined(VGCPU_COMPILER_MSVC)
#define VGCPU_NOINLINE __declspec(noinline)
#else
#define VGCPU_NOINLINE __attribute__((noinline))
#endif
