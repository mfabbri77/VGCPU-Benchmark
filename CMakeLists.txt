# Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
# SPDX-License-Identifier: MIT

# Blueprint Reference: Appendix A, §A.2 — Build System (CMake)
cmake_minimum_required(VERSION 3.21)

project(VGCPU-Benchmark
    VERSION 0.1.0
    DESCRIPTION "CPU-only 2D Vector Graphics Benchmark Suite"
    LANGUAGES CXX
)

# Blueprint Reference: Appendix A, §A.1 — C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# -----------------------------------------------------------------------------
# FetchContent for dependency management
# Blueprint Reference: Appendix A, §A.7.3 — Integration
# -----------------------------------------------------------------------------
include(FetchContent)

# nlohmann/json for manifest parsing
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# -----------------------------------------------------------------------------
# PlutoVG (FetchContent)
# Blueprint Reference: backends/plutovg.md
# -----------------------------------------------------------------------------
if(ENABLE_PLUTOVG)
    FetchContent_Declare(
        plutovg
        GIT_REPOSITORY https://github.com/sammycage/plutovg.git
        GIT_TAG        v1.3.2
    )
    FetchContent_MakeAvailable(plutovg)
    message(STATUS "PlutoVG fetched from GitHub (v1.3.2)")
endif()

# -----------------------------------------------------------------------------
# Cairo (System library via PkgConfig)
# Blueprint Reference: backends/cairo.md
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Blend2D & AsmJit (FetchContent)
# Blueprint Reference: backends/blend2d.md
# -----------------------------------------------------------------------------
if(ENABLE_BLEND2D)
    # AsmJit (Required by Blend2D)
    FetchContent_Declare(
        asmjit
        GIT_REPOSITORY https://github.com/asmjit/asmjit.git
        GIT_TAG        master
    )
    # Blend2D
    FetchContent_Declare(
        blend2d
        GIT_REPOSITORY https://github.com/blend2d/blend2d.git
        GIT_TAG        master
    )

    # Prevent GNUInstallDirs recursion
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "")
    set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "")
    set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "")

    set(ASMJIT_STATIC ON CACHE BOOL "" FORCE)
    set(ASMJIT_INSTALL OFF CACHE BOOL "" FORCE)
    set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
    set(BLEND2D_INSTALL OFF CACHE BOOL "" FORCE)
    set(BLEND2D_NO_JIT OFF CACHE BOOL "" FORCE)
    set(BLEND2D_EXTERNAL_ASMJIT OFF CACHE BOOL "" FORCE) # Use source directly

    # Populate asmjit manually
    FetchContent_GetProperties(asmjit)
    if(NOT asmjit_POPULATED)
        FetchContent_Populate(asmjit)
    endif()

    # Point Blend2D to asmjit source
    set(ASMJIT_DIR "${asmjit_SOURCE_DIR}" CACHE PATH "" FORCE)

    # Now build Blend2D (which will add_subdirectory asmjit)
    FetchContent_MakeAvailable(blend2d)
    
    message(STATUS "Blend2D & AsmJit fetched from GitHub")
endif()

# -----------------------------------------------------------------------------
# Skia (Prebuilt binaries from Aseprite m124)
# Blueprint Reference: backends/skia.md
# -----------------------------------------------------------------------------
if(ENABLE_SKIA)
    FetchContent_Declare(
        skia_binaries
        URL https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-macOS-Release-arm64.zip
    )
    FetchContent_MakeAvailable(skia_binaries)

    add_library(skia STATIC IMPORTED)
    set_target_properties(skia PROPERTIES
        IMPORTED_LOCATION "${skia_binaries_SOURCE_DIR}/out/Release-arm64/libskia.a"
        INTERFACE_INCLUDE_DIRECTORIES "${skia_binaries_SOURCE_DIR}"
    )

    if(APPLE)
        find_library(CORE_FOUNDATION CoreFoundation)
        find_library(CORE_GRAPHICS CoreGraphics)
        find_library(CORE_TEXT CoreText)
        find_library(CORE_SERVICES CoreServices)
        
        find_package(ZLIB REQUIRED)

        target_link_libraries(skia INTERFACE 
            ${CORE_FOUNDATION} 
            ${CORE_GRAPHICS} 
            ${CORE_TEXT} 
            ${CORE_SERVICES}
            ZLIB::ZLIB
        )
    endif()

    message(STATUS "Skia fetched (Prebuilt m124)")
endif()

# -----------------------------------------------------------------------------
# Cairo (System library via PkgConfig)
# Blueprint Reference: backends/cairo.md
# -----------------------------------------------------------------------------
if(ENABLE_CAIRO)
    # 1. Try PkgConfig (standard on Linux/macOS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO IMPORTED_TARGET cairo>=1.16.0)
    endif()

    # 2. If not found, try CMake Config (vcpkg on Windows)
    if(NOT CAIRO_FOUND)
        find_package(cairo CONFIG QUIET)
        if(cairo_FOUND)
            if(TARGET cairo::cairo)
                # Alias to PkgConfig::CAIRO so the rest of the build works transparently
                if(NOT TARGET PkgConfig::CAIRO)
                    add_library(PkgConfig::CAIRO ALIAS cairo::cairo)
                endif()
                set(CAIRO_FOUND TRUE)
            endif()
        endif()
    endif()

    # 3. If still not found, try Local Deps (Preshing binaries)
    if(NOT CAIRO_FOUND)
        set(LOCAL_CAIRO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/cairo/cairo-windows-1.17.2")
        if(EXISTS "${LOCAL_CAIRO_ROOT}/include/cairo.h")
            message(STATUS "Found local Cairo at: ${LOCAL_CAIRO_ROOT}")
            
            # Create imported target
            if(NOT TARGET cairo::cairo)
                add_library(cairo::cairo SHARED IMPORTED)
                
                if(MSVC)
                    set_target_properties(cairo::cairo PROPERTIES
                        IMPORTED_IMPLIB "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.lib"
                        IMPORTED_LOCATION "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll"
                        INTERFACE_INCLUDE_DIRECTORIES "${LOCAL_CAIRO_ROOT}/include"
                    )
                else()
                    # Assume x64 for now, adapt if needed
                    set_target_properties(cairo::cairo PROPERTIES
                        IMPORTED_IMPLIB "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.lib"
                        IMPORTED_LOCATION "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll"
                        INTERFACE_INCLUDE_DIRECTORIES "${LOCAL_CAIRO_ROOT}/include"
                    )
                endif()
            endif()

            # Alias
            if(NOT TARGET PkgConfig::CAIRO)
                add_library(PkgConfig::CAIRO ALIAS cairo::cairo)
            endif()
            
            set(CAIRO_FOUND TRUE)
            set(cairo_VERSION "1.17.2")
            
            # Save DLL location for copy step later
            set(LOCAL_CAIRO_DLL "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll")
        endif()
    endif()

    if(CAIRO_FOUND)
        message(STATUS "Cairo found (Version: ${cairo_VERSION})")
    else()
        message(FATAL_ERROR "Cairo not found. Install libcairo2-dev (Linux), use Homebrew (macOS), or vcpkg/deps (Windows).")
    endif()
endif()

# -----------------------------------------------------------------------------
# Backend Enable Options
# Blueprint Reference: Chapter 7, §7.2.5 — Build-Time Enablement
# -----------------------------------------------------------------------------
option(ENABLE_NULL_BACKEND    "Enable Null/Debug backend for testing" ON)
option(ENABLE_PLUTOVG         "Enable PlutoVG backend" OFF)
option(ENABLE_CAIRO           "Enable Cairo backend" OFF)
option(ENABLE_BLEND2D         "Enable Blend2D backend" OFF)
option(ENABLE_SKIA            "Enable Skia backend" OFF)
option(ENABLE_THORVG          "Enable ThorVG backend" OFF)
option(ENABLE_AGG             "Enable AGG backend" OFF)
option(ENABLE_QT              "Enable Qt Raster backend" OFF)
option(ENABLE_AMANITHVG       "Enable AmanithVG SRE backend" OFF)
option(ENABLE_RAQOTE          "Enable Raqote backend (Rust)" OFF)
option(ENABLE_VELLO_CPU       "Enable vello_cpu backend (Rust)" OFF)

# -----------------------------------------------------------------------------
# Compiler Warnings
# Blueprint Reference: Appendix D, §D.1 — Language and Compilation Standards
# -----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-Werror)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/WX)
    endif()
endif()

# -----------------------------------------------------------------------------
# MinGW Static Linking (Fix for missing libgcc/libstdc++ DLLs)
# -----------------------------------------------------------------------------
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# -----------------------------------------------------------------------------
# Core Library (common types, PAL, IR, Assets)
# -----------------------------------------------------------------------------
add_library(vgcpu_core STATIC
    src/common/status.cpp
    src/pal/timer.cpp
    src/pal/environment.cpp
    src/ir/ir_loader.cpp
    src/ir/prepared_scene.cpp
    src/assets/scene_registry.cpp
)

target_include_directories(vgcpu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vgcpu_core PUBLIC nlohmann_json::nlohmann_json)

# -----------------------------------------------------------------------------
# Adapter Library
# -----------------------------------------------------------------------------
set(ADAPTER_SOURCES "")

if(ENABLE_NULL_BACKEND)
    list(APPEND ADAPTER_SOURCES src/adapters/null/null_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_NULL_BACKEND)
endif()

if(ENABLE_PLUTOVG)
    list(APPEND ADAPTER_SOURCES src/adapters/plutovg/plutovg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_PLUTOVG)
endif()

if(ENABLE_CAIRO)
    list(APPEND ADAPTER_SOURCES src/adapters/cairo/cairo_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_CAIRO)
endif()

if(ENABLE_BLEND2D)
    list(APPEND ADAPTER_SOURCES src/adapters/blend2d/blend2d_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_BLEND2D)
endif()

if(ENABLE_SKIA)
    list(APPEND ADAPTER_SOURCES src/adapters/skia/skia_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_SKIA)
endif()

add_library(vgcpu_adapters STATIC
    src/adapters/adapter_registry.cpp
    ${ADAPTER_SOURCES}
)

target_link_libraries(vgcpu_adapters PUBLIC vgcpu_core)

# Link backend libraries
if(ENABLE_PLUTOVG)
    target_link_libraries(vgcpu_adapters PUBLIC plutovg)
endif()

if(ENABLE_CAIRO)
    target_link_libraries(vgcpu_adapters PUBLIC PkgConfig::CAIRO)
endif()

if(ENABLE_BLEND2D)
    target_link_libraries(vgcpu_adapters PUBLIC blend2d)
    target_include_directories(vgcpu_adapters PUBLIC ${blend2d_SOURCE_DIR})
endif()

if(ENABLE_SKIA)
    target_link_libraries(vgcpu_adapters PUBLIC skia)
endif()

# -----------------------------------------------------------------------------
# Harness Library
# -----------------------------------------------------------------------------
add_library(vgcpu_harness STATIC
    src/harness/harness.cpp
    src/harness/statistics.cpp
)

target_link_libraries(vgcpu_harness PUBLIC vgcpu_core vgcpu_adapters)

# -----------------------------------------------------------------------------
# Reporting Library
# -----------------------------------------------------------------------------
add_library(vgcpu_reporting STATIC
    src/reporting/json_writer.cpp
    src/reporting/csv_writer.cpp
    src/reporting/summary_writer.cpp
)

target_link_libraries(vgcpu_reporting PUBLIC vgcpu_core)

# -----------------------------------------------------------------------------
# CLI Executable
# Blueprint Reference: Chapter 7, §7.2.1 — Module cli
# -----------------------------------------------------------------------------
add_executable(vgcpu-benchmark
    src/cli/main.cpp
    src/cli/cli_parser.cpp
)

target_link_libraries(vgcpu-benchmark PRIVATE
    vgcpu_core
    vgcpu_adapters
    vgcpu_harness
    vgcpu_reporting
)

if(LOCAL_CAIRO_DLL)
    add_custom_command(TARGET vgcpu-benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_CAIRO_DLL}"
        $<TARGET_FILE_DIR:vgcpu-benchmark>
        COMMENT "Copying Cairo DLL to output directory"
    )
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
install(TARGETS vgcpu-benchmark RUNTIME DESTINATION bin)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "VGCPU-Benchmark Configuration Summary:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "  Enabled Backends:")
message(STATUS "    Null (Debug): ${ENABLE_NULL_BACKEND}")
message(STATUS "    PlutoVG:      ${ENABLE_PLUTOVG}")
message(STATUS "    Cairo:        ${ENABLE_CAIRO}")
message(STATUS "    Blend2D:      ${ENABLE_BLEND2D}")
message(STATUS "    Skia:         ${ENABLE_SKIA}")
message(STATUS "    ThorVG:       ${ENABLE_THORVG}")
message(STATUS "    AGG:          ${ENABLE_AGG}")
message(STATUS "    Qt:           ${ENABLE_QT}")
message(STATUS "    AmanithVG:    ${ENABLE_AMANITHVG}")
message(STATUS "    Raqote:       ${ENABLE_RAQOTE}")
message(STATUS "    vello_cpu:    ${ENABLE_VELLO_CPU}")
message(STATUS "")
