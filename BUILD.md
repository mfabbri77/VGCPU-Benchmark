# Build Instructions

## Prerequisites

*   **CMake**: 3.21 or later.
*   **Compiler**: C++20 compatible (GCC 10+, Clang 11+, MSVC 19.29+).
*   **Rust**: Stable toolchain (optional, required if enabling Raqote/Vello backends).
*   **Ninja**: Recommended build generator.

### Platform-Specific Dependencies

#### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential cmake ninja-build \
                     libcairo2-dev qt6-base-dev \
                     libfreetype6-dev libfontconfig1-dev
```

#### macOS
```bash
brew install cmake ninja cairo qt6 freetype fontconfig
```

#### Windows
*   Visual Studio 2019/2022 with C++ Desktop Development workload.
*   (Optional) **vcpkg** or **Chocolatey** for Cairo/Qt if not using prebuilts/system paths.
    *   Note: Skia binaries are downloaded automatically during configuration.

---

## Building

### 1. clone the repository
```bash
git clone https://github.com/mfabbri/VGCPU-Benchmark.git
cd VGCPU-Benchmark
```

### 2. Configure
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

**Common Options:**
*   `-DENABLE_SKIA=OFF`: Disable Skia backend (enabled by default).
*   `-DENABLE_QT=OFF`: Disable Qt backend (enabled by default).
*   `-DENABLE_RAQOTE=ON`: Enable Rust-based Raqote backend (needs Cargo).
*   `-DENABLE_VELLO_CPU=ON`: Enable Vello CPU backend (needs Cargo).

### 3. Build
```bash
cmake --build build --config Release
```

The executable will be located at `build/vgcpu-benchmark` (or `build\Release\vgcpu-benchmark.exe` on Windows).

## Troubleshooting

*   **Cairo not found**: Ensure `pkg-config` is in your environment or set `PKG_CONFIG_PATH`. On Windows, the build system looks for a local `deps/cairo` folder or standard installation paths.
*   **Qt6 not found**: Ensure `qmake` is in your PATH or set `Qt6_DIR`.
*   **Rust errors**: Ensure `cargo` is in your PATH if building with Rust bridges.
