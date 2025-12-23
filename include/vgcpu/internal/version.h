// include/vgcpu/internal/version.h
// Blueprint Reference: [TASK-05.01], [REQ-130], [REQ-131]
//
// Version information and stamping macros.

#pragma once

#include <string>

// Version components from CMake
#define VGCPU_VERSION_MAJOR 0
#define VGCPU_VERSION_MINOR 2
#define VGCPU_VERSION_PATCH 0

// Version string
#define VGCPU_VERSION_STRING "0.2.0"

// Version as integer for comparison (MAJOR*10000 + MINOR*100 + PATCH)
#define VGCPU_VERSION_NUMBER \
    ((VGCPU_VERSION_MAJOR * 10000) + (VGCPU_VERSION_MINOR * 100) + VGCPU_VERSION_PATCH)

// Report schema version per [REQ-133]
#define VGCPU_REPORT_SCHEMA_VERSION "0.1.0"

// Build info (set by CMake or defaults)
#ifndef VGCPU_GIT_COMMIT
#define VGCPU_GIT_COMMIT "unknown"
#endif

#ifndef VGCPU_BUILD_DATE
#define VGCPU_BUILD_DATE __DATE__
#endif

namespace vgcpu {
namespace version {

/// Get the version string (e.g., "0.2.0")
inline const char* GetVersionString() {
    return VGCPU_VERSION_STRING;
}

/// Get the major version number
inline int GetMajor() {
    return VGCPU_VERSION_MAJOR;
}

/// Get the minor version number
inline int GetMinor() {
    return VGCPU_VERSION_MINOR;
}

/// Get the patch version number
inline int GetPatch() {
    return VGCPU_VERSION_PATCH;
}

/// Get the git commit SHA (or "unknown" if not available)
inline const char* GetGitCommit() {
    return VGCPU_GIT_COMMIT;
}

/// Get the report schema version
inline const char* GetReportSchemaVersion() {
    return VGCPU_REPORT_SCHEMA_VERSION;
}

/// Check if the current version is at least (major.minor.patch)
inline bool IsAtLeast(int major, int minor, int patch) {
    int required = (major * 10000) + (minor * 100) + patch;
    return VGCPU_VERSION_NUMBER >= required;
}

}  // namespace version
}  // namespace vgcpu
