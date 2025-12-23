# cmake/vgcpu_deps.cmake
# -----------------------------------------------------------------------------
# Centralized Dependency Versions (Blueprint [REQ-99], [DEC-BUILD-05])
# This is the single source of truth for all external dependency versions.
# No floating branches (master/main) are allowed in shipped presets.
# -----------------------------------------------------------------------------

# nlohmann/json - JSON for Modern C++
set(VGCPU_DEP_NLOHMANN_JSON_TAG "v3.11.3")

# PlutoVG - Small C vector graphics library
set(VGCPU_DEP_PLUTOVG_TAG "v1.3.2")

# AsmJit - JIT assembler (used by Blend2D)
# Pinned to commit SHA for reproducibility (no release tags available)
set(VGCPU_DEP_ASMJIT_COMMIT "c87860217e43e2a06060fcaae5b468f6a55b9963")

# Blend2D - 2D vector graphics engine
# Pinned to commit SHA for reproducibility (rolling release, no version tags)
set(VGCPU_DEP_BLEND2D_COMMIT "6dbc2cefbc996379e07104e34519a440b49b15d7")

# Skia - 2D graphics library (prebuilt binaries from Aseprite)
set(VGCPU_DEP_SKIA_VERSION "m124-08a5439a6b")

# ThorVG - Vector graphics library
set(VGCPU_DEP_THORVG_TAG "v0.15.16")

# AGG - Anti-Grain Geometry
# Pinned to commit SHA (maintained fork, no version tags)
set(VGCPU_DEP_AGG_COMMIT "c4f36b4432142f22c0bf82c6fbdb41567a236be2")

# FreeType - Font rendering library (used by AGG)
set(VGCPU_DEP_FREETYPE_TAG "VER-2-13-2")

# AmanithVG SDK - Vector graphics library
# Pinned to commit SHA (SDK snapshots, no version tags)
set(VGCPU_DEP_AMANITHVG_COMMIT "b7f44f6b95b812adb07e4842d3cbee146bde801d")

# Cairo - Prebuilt for Windows
set(VGCPU_DEP_CAIRO_PREBUILT_VERSION "1.17.2")

# Corrosion - Rust-CMake bridge
set(VGCPU_DEP_CORROSION_TAG "v0.5.0")

# Rust toolchain version (stable, not nightly per [DEC-BUILD-06])
set(VGCPU_DEP_RUST_TOOLCHAIN "1.84.0")

# -----------------------------------------------------------------------------
# Validate no floating deps function (called at configure time)
# -----------------------------------------------------------------------------
function(vgcpu_validate_no_floating_deps)
    # This function checks that no dependency uses master/main
    # It's called from CMakeLists.txt after all deps are declared
    
    set(FLOATING_BRANCHES "master" "main" "develop" "trunk")
    set(DEPS_TO_CHECK
        "VGCPU_DEP_ASMJIT_COMMIT"
        "VGCPU_DEP_BLEND2D_COMMIT"
        "VGCPU_DEP_AGG_COMMIT"
        "VGCPU_DEP_AMANITHVG_COMMIT"
    )
    
    foreach(dep ${DEPS_TO_CHECK})
        foreach(branch ${FLOATING_BRANCHES})
            if("${${dep}}" STREQUAL "${branch}")
                message(FATAL_ERROR 
                    "[REQ-99] Floating dependency detected: ${dep}=${${dep}}\n"
                    "All dependencies must be pinned to tags or commit SHAs.\n"
                    "Update cmake/vgcpu_deps.cmake with a specific version.")
            endif()
        endforeach()
    endforeach()
    
    message(STATUS "[REQ-99] Dependency pinning validated - no floating branches")
endfunction()
