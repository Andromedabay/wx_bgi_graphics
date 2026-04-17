# wx_BGI_Graphics — Building from Source

This document covers everything needed to build wx_BGI_Graphics on Windows, Linux, and macOS.

See **[Tests.md](./Tests.md)** for how to run the automated test suite after building.  
See **[WxWidgets.md](./WxWidgets.md)** for wxWidgets-specific build flags and integration details.

---

## Dependencies

Most C/C++ dependencies are fetched automatically by CMake `FetchContent` at configure time.

> **macOS note:** FetchContent builds of GLFW 3.4 and wxWidgets 3.2.5 fail on
> macOS 15 / Apple Clang 17+ (see [macOS-specific build notes](#macos-apple-silicon--retina-displays)
> below). Install them via Homebrew and pass the system flags instead.

| Dependency | Version | Purpose |
|------------|---------|---------|
| GLFW | 3.4 | Window creation and OpenGL context (GLFW backend) |
| GLEW | 2.2.0 | OpenGL extension loading |
| GLM | 1.0.1 | Vector / matrix math for camera and UCS |
| wxWidgets | 3.2.5+ | Default rendering backend (embedded canvas) |
| nlohmann/json | 3.12.0 | DDS JSON serialization (`wx_bgi_dds.h`) |
| yaml-cpp | latest | DDS YAML serialization (`wx_bgi_dds.h`) |

**Optional tools** (not auto-fetched):

| Tool | Required for |
|------|-------------|
| Doxygen | API documentation (`api_docs` / `api_docs_pdf` CMake targets) |
| LaTeX / MiKTeX | PDF documentation (`api_docs_pdf` target) |
| FreePascal (fpc) | Pascal example programs and Pascal CTest targets |
| Python 3 | Python API coverage test (`bgi_api_coverage_python`) |

---

## Compilation Requirements

- **CMake 3.14** or later
- **C++20 compiler**: MSVC (Visual Studio 2019+), GCC 10+, or Clang 12+
- **OpenGL-capable GPU** (any desktop GPU from the last decade)
- **macOS**: Apple Clang 17+ (Xcode 16 / Command Line Tools); OpenGL 4.1 Core Profile required (all Apple Silicon M-series GPUs support this)

---

## Build Commands

### Windows (MSVC, multi-config)

```powershell
# Configure (auto-fetches all dependencies ~170 MB first time)
cmake -S . -B build

# Build Debug
cmake --build build -j --config Debug

# Build Release
cmake --build build -j --config Release
```

Optional CMake flags:

```powershell
# GLFW-only mode (skip wxWidgets, saves ~170 MB download)
cmake -S . -B build -DWXBGI_ENABLE_WX=OFF

# Enable keyboard injection test seams (CI / testing only)
cmake -S . -B build -DWXBGI_ENABLE_TEST_SEAMS=ON

# Stage OpenLB bridge assets (requires a local OpenLB tree)
cmake -S . -B build `
  -DWXBGI_ENABLE_OPENLB=ON `
  -DOPENLB_ROOT=C:\path\to\openlb\release
```

### Linux (GCC / Clang, single-config)

```bash
# Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Release
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j

# Optional: stage OpenLB bridge assets
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWXBGI_ENABLE_OPENLB=ON \
  -DOPENLB_ROOT=/path/to/openlb/release
cmake --build build --target openlb_bridge_package -j
```

### macOS (Clang, single-config)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

> **Apple Silicon / macOS 15+:** see the [macOS-specific notes](#macos-apple-silicon--retina-displays) section below for required extra flags.

---

## macOS Apple Silicon / Retina Displays

### Why system dependencies are required on macOS

Two FetchContent builds fail on macOS 15 with Apple Clang 17:

| Package | Failure reason |
|---------|----------------|
| GLFW 3.4 | Apple Clang 17 strict pointer-type checking rejects GLFW ObjC source |
| wxWidgets 3.2.5 | `CGDisplayCreateImage` is deprecated/removed in macOS 15 SDK |

Install system packages via Homebrew and pass override flags to CMake:

```bash
brew install glfw wxwidgets
```

Then configure with:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWXBGI_SYSTEM_GLFW=ON \
  -DWXBGI_SYSTEM_WX=ON

cmake --build build -j
```

For Release:

```bash
cmake -S . -B build_release \
  -DCMAKE_BUILD_TYPE=Release \
  -DWXBGI_SYSTEM_GLFW=ON \
  -DWXBGI_SYSTEM_WX=ON

cmake --build build_release -j
```

### OpenGL Core Profile

On macOS, the library requests **OpenGL 3.3 Core Profile** (GLFW backend) and **OpenGL 4.1 Core Profile** (wxWidgets backend). This is required because:

- macOS deprecates legacy GL contexts; a legacy context is capped at GL 2.1.
- On a GL 2.1 context `glGenVertexArrays` is NULL → shader/VAO path never initialises → the library falls back to `GL_POINTS` rasterization, which uses logical-pixel coordinates and renders only in the lower-left quarter of the window on Retina displays.
- All Apple Silicon M-series GPUs support up to OpenGL 4.1 Core Profile.

### FreePascal on macOS

FreePascal (FPC) can be installed via Homebrew:

```bash
brew install fpc
```

CMake detects the `fpc` binary automatically (checked for `arm64`/`aarch64` targets).

> **Known issue — FPC linker on macOS Homebrew install:** The Homebrew-installed
> `fpc.cfg` may reference `/opt/homebrew/Cellar/fpc/…` while the actual install
> is under `~/homebrew/Cellar/fpc/…` (or another non-standard prefix).  This
> causes `ld: library 'c' not found` at link time.  To work around it, create
> `~/.fpc.cfg` with the correct unit and library search paths pointing to your
> actual Homebrew prefix.  Pascal CTest targets will fail until this is resolved;
> all C++, Python, and wxWidgets tests are unaffected.

---

## Build Output Paths

After a successful build the library is at:

| Platform | Debug | Release |
|----------|-------|---------|
| Windows | `build\Debug\wx_bgi_opengl.dll` | `build\Release\wx_bgi_opengl.dll` |
| Linux | `build/libwx_bgi_opengl.so` | `build-rel/libwx_bgi_opengl.so` |
| macOS | `build/libwx_bgi_opengl.dylib` | `build-rel/libwx_bgi_opengl.dylib` |

Public headers installed under `build/install/include/`:

```
wx_bgi.h        Classic BGI C API
wx_bgi_ext.h    Extension API (wxbgi_* helpers)
wx_bgi_openlb.h Header-only OpenLB-style live-loop helpers
wx_bgi_3d.h     Camera, UCS, world-coordinate API
wx_bgi_dds.h    Drawing Description Data Structure (DDS) API
bgi_types.h     Shared types, structs, BGI constants, Camera3D
```

---

## Generating API Documentation

Documentation requires **Doxygen** to be installed and on `PATH`.

```powershell
# HTML only
cmake --build build --target api_docs -j --config Debug

# HTML + PDF (requires LaTeX)
cmake --build build --target api_docs_pdf -j --config Debug
```

Output locations:

| Format | Path |
|--------|------|
| HTML (Debug) | `build/doxygen/html/index.html` |
| HTML (Release, Linux/macOS) | `build-rel/doxygen/html/index.html` |
| PDF (Debug) | `build/doxygen/latex/refman.pdf` |
| PDF (Release, Linux/macOS) | `build-rel/doxygen/latex/refman.pdf` |

The Doxygen source is `docs/Doxyfile.in` — processed by CMake to substitute `@CMAKE_SOURCE_DIR@` and `@DOXYGEN_OUTPUT_DIR@` paths.

---

## Running Examples After Building

### Windows

```powershell
# C++ API coverage (draws, then closes automatically)
.\build\Debug\bgi_api_coverage_cpp.exe

# Keyboard queue demo (interactive — press keys, close window to exit)
.\build\Debug\wxbgi_keyboard_queue_cpp.exe

# Camera demo (interactive — W/A/S/D pan, +/- zoom, arrow keys orbit, Esc to exit)
.\build\Debug\wxbgi_camera_demo_cpp.exe

# OpenLB-style live loop demo
.\build\Debug\wxbgi_openlb_live_demo.exe

# wx embedded canvas coverage test (~4 s, exits automatically)
.\build\Debug\wx_bgi_canvas_coverage_test.exe

# wx 3D orbit test (~5 s, exits automatically)
.\build\Debug\wx_bgi_3d_orbit_test.exe

# Pascal coverage demo (requires FreePascal build step first)
.\build\pascal_coverage\demo_bgi_api_coverage.exe

# Pascal wx-canvas coverage demo (requires FreePascal build step first)
.\build\pascal_canvas_coverage\demo_bgi_canvas_coverage.exe

# Python API coverage (pass DLL path as argument)
python examples\python\bgi_api_coverage.py build\Debug\wx_bgi_opengl.dll
```

### Linux / macOS

```bash
# Export shared library path if needed
export LD_LIBRARY_PATH="$PWD/build:$LD_LIBRARY_PATH"      # Linux
export DYLD_LIBRARY_PATH="$PWD/build:$DYLD_LIBRARY_PATH"  # macOS

./build/bgi_api_coverage_cpp
./build/wxbgi_camera_demo_cpp
./build/wxbgi_openlb_live_demo
./build/wx_bgi_canvas_coverage_test
./build/wx_bgi_3d_orbit_test

# Python test (pass the .dylib path on macOS)
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.dylib  # macOS
python3 examples/python/bgi_api_coverage.py build/libwx_bgi_opengl.so     # Linux
```

> **Note:** The camera demo also accepts `--test` (`wxbgi_camera_demo_cpp --test`) to render one frame and exit immediately — this is the mode used by CTest.

## OpenLB Integration Workflow

OpenLB remains the simulation owner. The intended runtime pattern is:

1. create a standalone wx-backed viewer with `wxbgi_openlb_begin_session(...)`
2. advance the OpenLB solver in your own loop
3. draw the latest scalar/vector snapshot with `wxbgi_field_draw_scalar_grid(...)`
   and `wxbgi_field_draw_vector_grid(...)`
4. keep the viewer responsive with `wxbgi_openlb_present()` or `wxbgi_poll_events()`

When `WXBGI_ENABLE_OPENLB=ON`, the build adds the `openlb_bridge_package` target,
which stages the current shared library, public headers, and the live-loop demo in:

```text
build/openlb_bridge
```

This staging directory is meant to be copied or referenced from an OpenLB checkout
whose own examples are built with GNU Make.

---

## Building FreePascal Examples

FreePascal programs are built via CMake custom targets (not the normal C++ build).  
CMake detects `fpc` at configure time; if found, the Pascal targets are registered automatically.

```powershell
# Build all Pascal targets at once
cmake --build build --config Debug --target bgi_api_coverage_pascal_build
cmake --build build --config Debug --target bgi_canvas_coverage_pascal_build
cmake --build build --config Debug --target demo_bgi_wrapper_pascal_build
cmake --build build --config Debug --target demo_bgi_wrapper_gui_pascal_build
```

Each target compiles the `.pas` source and copies the up-to-date DLL into the output directory.  
See also: `examples/demoFreePascal/` and `examples/bgidemo-pascal/README.md`.

### Linux / macOS: Required `SetExceptionMask` in Pascal Programs

On Linux and macOS, GTK loads `librsvg` for icon rendering.  `librsvg` performs
floating-point operations (NaN / denormal) that violate FreePascal's default
strict x87 FPU mask and raise an `EInvalidOp` exception.

Every Pascal program that opens a wx window (uses `wxbgi_wx_app_create`) must
mask FPU exceptions **before** any GTK initialisation:

```pascal
uses SysUtils, Math;   { add Math to uses clause }

begin
  SetExceptionMask([exInvalidOp, exDenormalized, exZeroDivide,
                    exOverflow, exUnderflow, exPrecision]);
  wxbgi_wx_app_create;
  ...
```

The bundled test programs (`demo_bgi_api_coverage.pas`,
`demo_bgi_canvas_coverage.pas`, `test_input_hooks.pas`) already include this
call.  This fix is harmless on Windows (GTK is not used) and can be included
unconditionally in portable Pascal programs.
