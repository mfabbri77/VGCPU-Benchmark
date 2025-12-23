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

# Rust toolchain version (updated to nightly to support vello_cpu's edition2024 requirement)
if(APPLE)
    set(VGCPU_DEP_RUST_TOOLCHAIN "nightly-aarch64-apple-darwin")
elseif(WIN32)
    set(VGCPU_DEP_RUST_TOOLCHAIN "nightly-x86_64-pc-windows-msvc")
else()
    set(VGCPU_DEP_RUST_TOOLCHAIN "nightly-x86_64-unknown-linux-gnu")
endif()

# -----------------------------------------------------------------------------
# Vendored Dependencies (v1.1) - Integrity Check Constants
# -----------------------------------------------------------------------------
# ssim_lomont.hpp
set(VGCPU_DEP_SSIM_LOMONT_SHA256 "acd47dad46a781a8de112e58ae2c54641e83899f8abc198a55d551fd595f14bc")

# stb_image_write.h
set(VGCPU_DEP_STB_IMAGE_WRITE_SHA256 "cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05")

# stb_image.h
set(VGCPU_DEP_STB_IMAGE_READ_SHA256 "594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3")

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
